#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Transformer.hpp"
#include "sim/PowerSolver.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

/*
		please just refer to https://www.ijert.org/research/load-flow-solution-u-sing-simplified-newton-raphson-method-IJERTV2IS121281.pdf
		eq 1-17, TODO, figure out why this implementation doesn't work
*/
namespace GLStation::Simulation {

/*
		rebuilds admittance matrix upon update, kinda redundant atm
*/
std::unique_ptr<Util::SparseMatrix<std::complex<Core::f64>>>
	PowerSolver::s_yBus = nullptr;
static bool s_busDiscard = true;
static std::vector<Grid::Node *> s_busNodes;
static std::vector<Grid::Load *> s_loadList;
static std::vector<Grid::Generator *> s_generatorList;
static std::vector<Grid::Line *> s_lineList;
static std::vector<Grid::Transformer *> s_trafoList;
static size_t s_slackIdx = 0;
static std::vector<char> s_energisedBus;
static std::vector<std::vector<Core::f64>> s_Jbuf;

Core::f64 PowerSolver::sBaseKw() { return 100000.0; }
Core::f64 PowerSolver::sBaseMva() { return 100.0; }

/*
		the Ybus admittance matrix populator yayy!!! 
		zbase = V^2/Sbase per node, Y = 1/Z per branch in pu lines add y between m_from and m_to
*/
static bool sameUndirectedEndpoints(Grid::Node *a, Grid::Node *b, Grid::Node *c,
									Grid::Node *d) {
	if (!a || !b || !c || !d)
		return false;
	return (a == c && b == d) || (a == d && b == c);
}

static bool
lineSkippedByOpenBreaker(Grid::Line *line,
						 const std::vector<Grid::Breaker *> &breakers) {
	auto from = line->getFromNode();
	auto to = line->getToNode();
	for (auto brk : breakers) {
		if (!brk->isOpen())
			continue;
		if (sameUndirectedEndpoints(from, to, brk->getFromNode(),
									brk->getToNode()))
			return true;
	}
	return false;
}

static void markIslandsAndEnergizationImpl(
	const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
		&substations) {
	std::vector<Grid::Line *> allLines;
	std::vector<Grid::Transformer *> allTrafos;
	std::vector<Grid::Breaker *> allBreakers;
	for (const auto &sub : substations) {
		for (const auto &comp : sub->getComponents()) {
			if (auto line = dynamic_cast<Grid::Line *>(comp.get()))
				allLines.push_back(line);
			else if (auto trafo = dynamic_cast<Grid::Transformer *>(comp.get()))
				allTrafos.push_back(trafo);
			else if (auto brk = dynamic_cast<Grid::Breaker *>(comp.get()))
				allBreakers.push_back(brk);
		}
	}
	std::map<Grid::Node *, std::vector<Grid::Node *>> adj;
	auto addEdge = [&](Grid::Node *a, Grid::Node *b) {
		if (!a || !b)
			return;
		adj[a].push_back(b);
		adj[b].push_back(a);
	};
	for (auto line : allLines) {
		if (lineSkippedByOpenBreaker(line, allBreakers))
			continue;
		addEdge(line->getFromNode(), line->getToNode());
	}
	for (auto trafo : allTrafos) {
		addEdge(trafo->getPrimaryNode(), trafo->getSecondaryNode());
	}
	for (auto brk : allBreakers) {
		if (brk->isOpen())
			continue;
		bool paired = false;
		for (auto line : allLines) {
			if (sameUndirectedEndpoints(brk->getFromNode(), brk->getToNode(),
										line->getFromNode(),
										line->getToNode())) {
				paired = true;
				break;
			}
		}
		if (!paired)
			addEdge(brk->getFromNode(), brk->getToNode());
	}
	s_energisedBus.assign(s_busNodes.size(), 0);
	if (s_slackIdx >= s_busNodes.size())
		return;
	Grid::Node *slackNode = s_busNodes[s_slackIdx];
	std::queue<Grid::Node *> q;
	std::map<Grid::Node *, char> vis;
	q.push(slackNode);
	vis[slackNode] = 1;
	while (!q.empty()) {
		Grid::Node *u = q.front();
		q.pop();
		auto it = adj.find(u);
		if (it == adj.end())
			continue;
		for (Grid::Node *v : it->second) {
			if (!vis[v]) {
				vis[v] = 1;
				q.push(v);
			}
		}
	}
	for (size_t i = 0; i < s_busNodes.size(); ++i) {
		bool ok = vis.count(s_busNodes[i]) != 0;
		s_energisedBus[i] = ok ? 1 : 0;
		s_busNodes[i]->setEnergised(ok);
		if (!ok)
			s_busNodes[i]->setVoltage(std::complex<Core::f64>(0.0, 0.0));
	}
}

static size_t findSlackBusIndex() {
	for (size_t i = 0; i < s_busNodes.size(); ++i) {
		for (auto gen : s_generatorList) {
			if (gen->getConnectedNode() == s_busNodes[i] &&
				gen->getMode() == Grid::GeneratorMode::Slack)
				return i;
		}
	}
	return 0;
}

void PowerSolver::updateGeneratorQFromSolution() {
	if (!s_yBus || s_busNodes.empty())
		return;
	size_t n = s_busNodes.size();
	const auto &vals = s_yBus->getValues();
	const auto &cols = s_yBus->getColIndex();
	const auto &rowPtr = s_yBus->getRowPtr();
	for (auto gen : s_generatorList) {
		if (gen->getMode() != Grid::GeneratorMode::PV)
			continue;
		Grid::Node *node = gen->getConnectedNode();
		if (!node)
			continue;
		size_t si = n;
		for (size_t i = 0; i < n; ++i) {
			if (s_busNodes[i] == node) {
				si = i;
				break;
			}
		}
		if (si >= n)
			continue;
		std::complex<Core::f64> Vi = s_busNodes[si]->getVoltage();
		std::complex<Core::f64> Ii(0, 0);
		for (size_t k = rowPtr[si]; k < rowPtr[si + 1]; ++k)
			Ii += vals[k] * s_busNodes[cols[k]]->getVoltage();
		std::complex<Core::f64> Si = Vi * std::conj(Ii);
		Core::f64 qLoadKvar = 0;
		for (auto load : s_loadList) {
			if (load->getConnectedNode() != node)
				continue;
			Core::f64 P = load->getCurrentConsumption();
			Core::f64 pf = load->getPowerFactor();
			if (pf > 1e-6)
				qLoadKvar += P * std::sqrt(std::max(0.0, 1.0 - pf * pf)) / pf;
		}
		Core::f64 qGenKw = std::imag(Si) * sBaseKw() + qLoadKvar;
		gen->setActualQKw(qGenKw);
	}
}

bool PowerSolver::resolvePvQLimits() {
	bool any = false;
	for (auto gen : s_generatorList) {
		if (gen->getMode() != Grid::GeneratorMode::PV) {
			gen->setPvQClampActive(false, 0.0);
			continue;
		}
		Core::f64 q = gen->getActualQKw();
		if (q > gen->getMaxQ()) {
			gen->setPvQClampActive(true, gen->getMaxQ());
			any = true;
		} else if (q < gen->getMinQ()) {
			gen->setPvQClampActive(true, gen->getMinQ());
			any = true;
		} else
			gen->setPvQClampActive(false, 0.0);
	}
	return any;
}

void PowerSolver::solve(
	const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
		&substations,
	const SolverSettings &settings) {
	for (auto gen : s_generatorList)
		gen->setPvQClampActive(false, 0.0);
	for (int outer = 0; outer < 8; ++outer) {
		if (s_busDiscard || !s_yBus || s_busNodes.empty()) {
			buildYBus(substations);
			s_busDiscard = false;
		}
		if (!s_yBus || s_busNodes.empty())
			return;
		s_slackIdx = findSlackBusIndex();
		markIslandsAndEnergizationImpl(substations);
		for (::GLStation::Core::u32 i = 0; i < settings.maxIterations; ++i) {
			if (runIteration(settings))
				break;
		}
		updateSlackGeneratorPowerFromSolution();
		updateGeneratorQFromSolution();
		syncBranchFlowsFromPUSolution();
		if (!resolvePvQLimits())
			break;
	}
}

void PowerSolver::invalidateYBus() {
	s_busDiscard = true;
	s_yBus.reset();
}

void PowerSolver::buildYBus(
	const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
		&substations) {
	s_busNodes.clear();
	s_loadList.clear();
	s_generatorList.clear();
	s_lineList.clear();
	s_trafoList.clear();
	std::vector<Grid::Line *> allLines;
	std::vector<Grid::Transformer *> allTransformers;
	std::vector<Grid::Breaker *> allBreakers;
	std::vector<Grid::Shunt *> allShunts;

	for (const auto &sub : substations) {
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				s_busNodes.push_back(node);
			} else if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
				allLines.push_back(line);
				s_lineList.push_back(line);
			} else if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				s_loadList.push_back(load);
			} else if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				s_generatorList.push_back(gen);
			} else if (auto trafo =
						   dynamic_cast<Grid::Transformer *>(comp.get())) {
				allTransformers.push_back(trafo);
				s_trafoList.push_back(trafo);
			} else if (auto breaker =
						   dynamic_cast<Grid::Breaker *>(comp.get())) {
				allBreakers.push_back(breaker);
			} else if (auto sh = dynamic_cast<Grid::Shunt *>(comp.get())) {
				allShunts.push_back(sh);
			}
		}
	}

	if (s_busNodes.empty()) {
		s_yBus.reset();
		return;
	}

	size_t busCount = s_busNodes.size();
	std::map<Grid::Node *, size_t> nodeToIndex;
	std::map<Grid::Node *, Core::f64> nodeZBase;

	for (size_t i = 0; i < busCount; ++i) {
		nodeToIndex[s_busNodes[i]] = i;
		Core::f64 vbase = s_busNodes[i]->getBaseVoltage();
		nodeZBase[s_busNodes[i]] = (vbase * vbase) / (sBaseKw() / 1000.0);
	}

	std::map<std::pair<size_t, size_t>, std::complex<Core::f64>> y_temp;

	for (auto line : allLines) {
		auto from = line->getFromNode();
		auto to = line->getToNode();
		if (!from || !to)
			continue;
		bool skipLine = false;
		for (auto brk : allBreakers) {
			if (!brk->isOpen())
				continue;
			if (sameUndirectedEndpoints(from, to, brk->getFromNode(),
										brk->getToNode())) {
				skipLine = true;
				break;
			}
		}
		if (skipLine)
			continue;

		size_t i = nodeToIndex[from];
		size_t j = nodeToIndex[to];

		Core::f64 zbase = nodeZBase[from];
		std::complex<Core::f64> z_pu(line->getResistance() / zbase,
									 line->getReactance() / zbase);
		std::complex<Core::f64> y =
			(std::abs(z_pu) > 1e-9) ? (std::complex<Core::f64>(1.0, 0.0) / z_pu)
									: 0.0;

		y_temp[{i, j}] -= y;
		y_temp[{j, i}] -= y;
		y_temp[{i, i}] += y;
		y_temp[{j, j}] += y;

		Core::f64 btot = line->getLineChargingSusceptancePu();
		if (std::abs(btot) > 1e-12) {
			Core::f64 bh = btot * 0.5;
			std::complex<Core::f64> jb(0.0, bh);
			y_temp[{i, i}] += jb;
			y_temp[{j, j}] += jb;
		}
	}

	for (auto trafo : allTransformers) {
		auto pri = trafo->getPrimaryNode();
		auto sec = trafo->getSecondaryNode();
		if (!pri || !sec)
			continue;

		size_t i = nodeToIndex[pri];
		size_t j = nodeToIndex[sec];

		Core::f64 zbase = nodeZBase[pri];
		std::complex<Core::f64> z_pu(trafo->getResistance() / zbase,
									 trafo->getReactance() / zbase);
		std::complex<Core::f64> y =
			(std::abs(z_pu) > 1e-9) ? (std::complex<Core::f64>(1.0, 0.0) / z_pu)
									: 0.0;

		Core::f64 ratio = pri->getBaseVoltage() / sec->getBaseVoltage();
		Core::f64 ph = trafo->getPhaseShiftDeg() * Core::PI / 180.0;
		std::complex<Core::f64> a = std::polar(trafo->getTap() / ratio, ph);
		if (std::abs(a) < 1e-9)
			a = 1.0;

		y_temp[{i, i}] += y / (a * std::conj(a));
		y_temp[{j, j}] += y;
		y_temp[{i, j}] -= y / std::conj(a);
		y_temp[{j, i}] -= y / a;
	}

	for (auto breaker : allBreakers) {
		if (breaker->isOpen())
			continue;

		auto from = breaker->getFromNode();
		auto to = breaker->getToNode();
		if (!from || !to)
			continue;

		bool pairedWithLine = false;
		for (auto line : allLines) {
			if (sameUndirectedEndpoints(from, to, line->getFromNode(),
										line->getToNode())) {
				pairedWithLine = true;
				break;
			}
		}
		if (pairedWithLine)
			continue;

		size_t i = nodeToIndex[from];
		size_t j = nodeToIndex[to];

		std::complex<Core::f64> y(1e6, 0.0);

		y_temp[{i, j}] -= y;
		y_temp[{j, i}] -= y;
		y_temp[{i, i}] += y;
		y_temp[{j, j}] += y;
	}

	for (auto sh : allShunts) {
		Grid::Node *n = sh->getNode();
		if (!n || !nodeToIndex.count(n))
			continue;
		size_t k = nodeToIndex[n];
		std::complex<Core::f64> ysh(sh->getGPu(), sh->getBPu());
		y_temp[{k, k}] += ysh;
	}

	std::vector<std::complex<Core::f64>> values;
	std::vector<size_t> colIndex;
	std::vector<size_t> rowPtr;

	rowPtr.push_back(0);
	for (size_t i = 0; i < busCount; ++i) {
		for (size_t j = 0; j < busCount; ++j) {
			if (y_temp.count({i, j})) {
				values.push_back(y_temp[{i, j}]);
				colIndex.push_back(j);
			}
		}
		rowPtr.push_back(values.size());
	}

	s_yBus = std::make_unique<Util::SparseMatrix<std::complex<Core::f64>>>(
		busCount, busCount);
	s_yBus->setDirect(values, colIndex, rowPtr);
}

void PowerSolver::syncBranchFlowsFromPUSolution() {
	for (auto line : s_lineList) {
		auto from = line->getFromNode();
		auto to = line->getToNode();
		if (!from || !to)
			continue;
		std::complex<Core::f64> Vf = from->getVoltage();
		std::complex<Core::f64> Vt = to->getVoltage();
		Core::f64 zbase = (from->getBaseVoltage() * from->getBaseVoltage()) /
						  (sBaseKw() / 1000.0);
		std::complex<Core::f64> z_pu(line->getResistance() / zbase,
									 line->getReactance() / zbase);
		std::complex<Core::f64> y_s =
			(std::abs(z_pu) > 1e-12) ? (1.0 / z_pu) : 0.0;
		Core::f64 bh = line->getLineChargingSusceptancePu() * 0.5;
		std::complex<Core::f64> Ipu =
			(Vf - Vt) * y_s + Vf * std::complex<Core::f64>(0.0, bh);
		line->setFlowFromSolverPu(Ipu, 0.0);
	}
	for (auto trafo : s_trafoList) {
		auto pri = trafo->getPrimaryNode();
		auto sec = trafo->getSecondaryNode();
		if (!pri || !sec)
			continue;
		std::complex<Core::f64> Vi = pri->getVoltage();
		std::complex<Core::f64> Vj = sec->getVoltage();
		Core::f64 zbase = (pri->getBaseVoltage() * pri->getBaseVoltage()) /
						  (sBaseKw() / 1000.0);
		std::complex<Core::f64> z_pu(trafo->getResistance() / zbase,
									 trafo->getReactance() / zbase);
		std::complex<Core::f64> y =
			(std::abs(z_pu) > 1e-12) ? (1.0 / z_pu) : 0.0;
		Core::f64 ratio = pri->getBaseVoltage() / sec->getBaseVoltage();
		Core::f64 ph = trafo->getPhaseShiftDeg() * Core::PI / 180.0;
		std::complex<Core::f64> a = std::polar(trafo->getTap() / ratio, ph);
		std::complex<Core::f64> Ipu = (Vi / a - Vj) * y;
		Core::f64 sMva = std::abs(Vi * std::conj(Ipu)) * sBaseMva();
		Core::f64 vll = pri->getBaseVoltage() * std::abs(Vi);
		Core::f64 amps =
			(vll > 1e-9) ? (sMva * 1000.0 / (std::sqrt(3.0) * vll)) : 0.0;
		trafo->setFlowFromSolverAmps(amps);
	}
}

/*
	updates every tick properly now discarding invalid yBus calculations, massive oversight in the first place tbh
	also to do angle geometry  
*/
void PowerSolver::updateSlackGeneratorPowerFromSolution() {
	if (!s_yBus || s_busNodes.empty())
		return;
	size_t busCount = s_busNodes.size();
	const auto &vals = s_yBus->getValues();
	const auto &cols = s_yBus->getColIndex();
	const auto &rowPtr = s_yBus->getRowPtr();

	for (auto gen : s_generatorList) {
		if (gen->getMode() != Grid::GeneratorMode::Slack)
			continue;
		Grid::Node *node = gen->getConnectedNode();
		if (!node)
			continue;
		size_t si = 0;
		bool found = false;
		for (size_t i = 0; i < busCount; ++i) {
			if (s_busNodes[i] == node) {
				si = i;
				found = true;
				break;
			}
		}
		if (!found)
			continue;
		std::complex<Core::f64> Vi_pu = s_busNodes[si]->getVoltage();
		std::complex<Core::f64> Ii_pu(0, 0);
		for (size_t k = rowPtr[si]; k < rowPtr[si + 1]; ++k)
			Ii_pu += vals[k] * s_busNodes[cols[k]]->getVoltage();
		std::complex<Core::f64> Si_pu = Vi_pu * std::conj(Ii_pu);
		Core::f64 loadKw = 0;
		for (auto load : s_loadList) {
			if (load->getConnectedNode() == node)
				loadKw += load->getCurrentConsumption();
		}
		gen->setActualPowerKw(Si_pu.real() * sBaseKw() + loadKw);
	}
}

struct PowerMismatch {
	Core::f64 deltaP;
	Core::f64 deltaQ;
};

/*
		function for gaussian eliminiation, partial credit to https://ioinformatic.org/index.php/JAIEA/article/download/536/376 
*/
static void simpleSolve(std::vector<std::vector<Core::f64>> &A,
						std::vector<Core::f64> &b, std::vector<Core::f64> &x) {
	size_t n = b.size();
	for (size_t i = 0; i < n; i++) {
		size_t max = i;
		for (size_t k = i + 1; k < n; k++) {
			if (std::abs(A[k][i]) > std::abs(A[max][i]))
				max = k;
		}
		std::swap(A[i], A[max]);
		std::swap(b[i], b[max]);

		if (std::abs(A[i][i]) < 1e-12)
			continue;

		for (size_t k = i + 1; k < n; k++) {
			Core::f64 factor = A[k][i] / A[i][i];
			b[k] -= factor * b[i];
			for (size_t j = i; j < n; j++) {
				A[k][j] -= factor * A[i][j];
			}
		}
	}

	x.resize(n);
	for (size_t i = n; i-- > 0;) {
		Core::f64 sum = 0;
		for (size_t j = i + 1; j < n; j++)
			sum += A[i][j] * x[j];
		if (std::abs(A[i][i]) < 1e-12) {
			x[i] = 0;
			continue;
		}
		x[i] = (b[i] - sum) / A[i][i];
	}
}

/*
		i hope newton and raphson both rot in hell
		p and Q mismatch calculated from Y*V vs expected P Q divergence from loads 
		Q = P sqrt(1-pf^2)/pf for loads, use in jacobian matrix
		J dx = b for angle and voltage updates, then apply dx to PQ and PV buses until convergence, return if mismatches have been eliminiated
		https://www.ijert.org/research/load-flow-solution-u-sing-simplified-newton-raphson-method-IJERTV2IS121281.pdf
*/
bool PowerSolver::runIteration(const SolverSettings &settings) {
	(void)settings;
	if (!s_yBus)
		return true;
	size_t n = s_busNodes.size();
	s_slackIdx = findSlackBusIndex();

	std::vector<size_t> pq_indices, pv_indices;
	size_t slack_idx = s_slackIdx;

	for (size_t i = 0; i < n; ++i) {
		if (!s_energisedBus.empty() && i < s_energisedBus.size() &&
			!s_energisedBus[i]) {
			continue;
		}
		auto node = s_busNodes[i];
		bool isSlack = false;
		bool isPV = false;
		for (auto gen : s_generatorList) {
			if (gen->getConnectedNode() != node)
				continue;
			if (gen->getMode() == Grid::GeneratorMode::Slack)
				isSlack = true;
			else if (gen->getMode() == Grid::GeneratorMode::PV &&
					 !gen->isPvQClampActive())
				isPV = true;
		}
		if (isSlack)
			slack_idx = i;
		else if (isPV)
			pv_indices.push_back(i);
		else
			pq_indices.push_back(i);
	}

	std::vector<PowerMismatch> mismatches(n);

	for (auto gen : s_generatorList) {
		if (gen->getMode() == Grid::GeneratorMode::Slack) {
			size_t idx = 0;
			for (size_t i = 0; i < n; ++i)
				if (s_busNodes[i] == gen->getConnectedNode()) {
					idx = i;
					break;
				}
			s_busNodes[idx]->setVoltage(std::polar(gen->getTargetV(), 0.0));
		}
	}

	for (size_t i = 0; i < n; ++i) {
		if (!s_energisedBus.empty() && i < s_energisedBus.size() &&
			!s_energisedBus[i]) {
			mismatches[i].deltaP = 0;
			mismatches[i].deltaQ = 0;
			continue;
		}
		std::complex<Core::f64> Vi_pu = s_busNodes[i]->getVoltage();
		std::complex<Core::f64> Ii_pu(0, 0);

		const auto &vals = s_yBus->getValues();
		const auto &cols = s_yBus->getColIndex();
		const auto &rowPtr = s_yBus->getRowPtr();

		for (size_t k = rowPtr[i]; k < rowPtr[i + 1]; ++k)
			Ii_pu += vals[k] * s_busNodes[cols[k]]->getVoltage();

		std::complex<Core::f64> Si_pu = Vi_pu * std::conj(Ii_pu);

		Core::f64 P_sched_pu = 0, Q_sched_pu = 0;
		for (auto gen : s_generatorList)
			if (gen->getConnectedNode() == s_busNodes[i]) {
				if (gen->getMode() != Grid::GeneratorMode::Slack)
					P_sched_pu += gen->getActualP() / sBaseKw();
				if (gen->getMode() == Grid::GeneratorMode::PV &&
					gen->isPvQClampActive())
					Q_sched_pu += gen->getQFixedKw() / sBaseKw();
			}
		for (auto load : s_loadList) {
			if (load->getConnectedNode() == s_busNodes[i]) {
				Core::f64 P = load->getCurrentConsumption();
				Core::f64 pf = load->getPowerFactor();
				P_sched_pu -= P / sBaseKw();
				Q_sched_pu -= (P * std::sqrt(1.0 - pf * pf) / pf) / sBaseKw();
			}
		}

		mismatches[i].deltaP = P_sched_pu - Si_pu.real();
		mismatches[i].deltaQ = Q_sched_pu - Si_pu.imag();
	}

	Core::f64 maxMismatch = 0;
	for (size_t i : pq_indices) {
		maxMismatch = std::max(maxMismatch, std::abs(mismatches[i].deltaP));
		maxMismatch = std::max(maxMismatch, std::abs(mismatches[i].deltaQ));
	}
	for (size_t i : pv_indices)
		maxMismatch = std::max(maxMismatch, std::abs(mismatches[i].deltaP));

	if (maxMismatch < settings.tolerance)
		return true;

	size_t n_pq = pq_indices.size();
	size_t n_pv = pv_indices.size();
	size_t dim = n_pq * 2 + n_pv;

	if (s_Jbuf.size() != dim)
		s_Jbuf.assign(dim, std::vector<Core::f64>(dim, 0.0));
	else {
		for (auto &row : s_Jbuf)
			std::fill(row.begin(), row.end(), 0.0);
	}
	std::vector<Core::f64> b(dim, 0.0);

	std::map<size_t, size_t> theta_map, v_map;
	for (size_t i = 0; i < n_pq; ++i) {
		theta_map[pq_indices[i]] = i;
		v_map[pq_indices[i]] = i + n_pq + n_pv;
	}
	for (size_t i = 0; i < n_pv; ++i) {
		theta_map[pv_indices[i]] = i + n_pq;
	}

	for (size_t i = 0; i < n; ++i) {
		if (i == slack_idx)
			continue;
		if (!s_energisedBus.empty() && i < s_energisedBus.size() &&
			!s_energisedBus[i])
			continue;
		if (!theta_map.count(i))
			continue;

		std::complex<Core::f64> Vi_pu = s_busNodes[i]->getVoltage();
		Core::f64 Vi = std::abs(Vi_pu);
		Core::f64 delta_i = std::arg(Vi_pu);

		Core::f64 Pi = 0, Qi = 0;
		const auto &vals = s_yBus->getValues();
		const auto &cols = s_yBus->getColIndex();
		const auto &rowPtr = s_yBus->getRowPtr();

		for (size_t k = rowPtr[i]; k < rowPtr[i + 1]; ++k) {
			size_t j = cols[k];
			std::complex<Core::f64> Vj_pu = s_busNodes[j]->getVoltage();
			Core::f64 Vj = std::abs(Vj_pu);
			Core::f64 delta_j = std::arg(Vj_pu);
			std::complex<Core::f64> Yij = vals[k];
			Core::f64 Gij = Yij.real();
			Core::f64 Bij = Yij.imag();
			Pi += Vi * Vj *
				  (Gij * cos(delta_i - delta_j) + Bij * sin(delta_i - delta_j));
			Qi += Vi * Vj *
				  (Gij * sin(delta_i - delta_j) - Bij * cos(delta_i - delta_j));
		}

		size_t row_theta = theta_map[i];
		b[row_theta] = mismatches[i].deltaP;
		if (v_map.count(i))
			b[v_map[i]] = mismatches[i].deltaQ;

		for (size_t k = rowPtr[i]; k < rowPtr[i + 1]; ++k) {
			size_t j = cols[k];
			if (j == slack_idx)
				continue;
			if (!s_energisedBus.empty() && j < s_energisedBus.size() &&
				!s_energisedBus[j])
				continue;
			if (!theta_map.count(j))
				continue;

			std::complex<Core::f64> Vj_pu = s_busNodes[j]->getVoltage();
			Core::f64 Vj = std::abs(Vj_pu);
			Core::f64 delta_j = std::arg(Vj_pu);
			std::complex<Core::f64> Yij = vals[k];
			Core::f64 Gij = Yij.real();
			Core::f64 Bij = Yij.imag();
			Core::f64 dij = delta_i - delta_j;

			if (i == j) {
				s_Jbuf[row_theta][row_theta] = -Qi - Bij * Vi * Vi;
				if (v_map.count(i)) {
					size_t row_v = v_map[i];
					s_Jbuf[row_theta][row_v] = (Pi + Gij * Vi * Vi) / Vi;
					s_Jbuf[row_v][row_theta] = Pi - Gij * Vi * Vi;
					s_Jbuf[row_v][row_v] = (Qi - Bij * Vi * Vi) / Vi;
				}
			} else {
				size_t col_theta = theta_map[j];
				s_Jbuf[row_theta][col_theta] =
					Vi * Vj * (Gij * sin(dij) - Bij * cos(dij));
				if (v_map.count(i)) {
					size_t row_v = v_map[i];
					s_Jbuf[row_v][col_theta] =
						-Vi * Vj * (Gij * cos(dij) + Bij * sin(dij));
					if (v_map.count(j)) {
						size_t col_v = v_map[j];
						s_Jbuf[row_theta][col_v] =
							Vi * (Gij * cos(dij) + Bij * sin(dij));
						s_Jbuf[row_v][col_v] =
							Vi * (Gij * sin(dij) - Bij * cos(dij));
					}
				} else if (v_map.count(j)) {
					size_t col_v = v_map[j];
					s_Jbuf[row_theta][col_v] =
						Vi * (Gij * cos(dij) + Bij * sin(dij));
				}
			}
		}
	}

	std::vector<Core::f64> dx;
	simpleSolve(s_Jbuf, b, dx);

	for (size_t i : pq_indices) {
		Core::f64 angle =
			std::arg(s_busNodes[i]->getVoltage()) + dx[theta_map[i]];
		Core::f64 mag_pu = std::abs(s_busNodes[i]->getVoltage()) + dx[v_map[i]];
		mag_pu = std::clamp(mag_pu, 0.5, 1.5);
		s_busNodes[i]->setVoltage(std::polar(mag_pu, angle));
	}
	for (size_t i : pv_indices) {
		Core::f64 angle =
			std::arg(s_busNodes[i]->getVoltage()) + dx[theta_map[i]];
		Core::f64 mag_pu = std::abs(s_busNodes[i]->getVoltage());
		s_busNodes[i]->setVoltage(std::polar(mag_pu, angle));
	}

	return false;
}
/*
	angle fixes PER node, this changes with 3P
*/
} // namespace GLStation::Simulation
