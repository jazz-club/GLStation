#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Transformer.hpp"
#include "sim/PowerSolver.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

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
static bool s_topologyDirty = true;
static std::vector<Grid::Node *> s_nodes;
static std::vector<Grid::Load *> s_loads;
static std::vector<Grid::Generator *> s_generators;

void PowerSolver::solve(
	const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
		&substations,
	const SolverSettings &settings) {
	if (s_topologyDirty || !s_yBus || s_nodes.empty()) {
		buildYBus(substations);
		s_topologyDirty = false;
	}
	if (!s_yBus || s_nodes.empty())
		return;
	for (::GLStation::Core::u32 i = 0; i < settings.maxIterations; ++i) {
		if (runIteration())
			break;
	}
}

void PowerSolver::invalidateYBus() { s_topologyDirty = true; }

static const Core::f64 S_BASE = 100000.0;

/*
		the Ybus admittance matrix populator yayy!!! 
		zbase = V^2/Sbase per node, Y = 1/Z per branch in pu lines add y between m_from and m_to
*/
void PowerSolver::buildYBus(
	const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
		&substations) {
	s_nodes.clear();
	s_loads.clear();
	s_generators.clear();
	std::vector<Grid::Line *> allLines;
	std::vector<Grid::Transformer *> allTransformers;
	std::vector<Grid::Breaker *> allBreakers;

	for (const auto &sub : substations) {
		for (const auto &comp : sub->getComponents()) {
			if (auto node = dynamic_cast<Grid::Node *>(comp.get())) {
				s_nodes.push_back(node);
			} else if (auto line = dynamic_cast<Grid::Line *>(comp.get())) {
				allLines.push_back(line);
			} else if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				s_loads.push_back(load);
			} else if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				s_generators.push_back(gen);
			} else if (auto trafo =
						   dynamic_cast<Grid::Transformer *>(comp.get())) {
				allTransformers.push_back(trafo);
			} else if (auto breaker =
						   dynamic_cast<Grid::Breaker *>(comp.get())) {
				allBreakers.push_back(breaker);
			}
		}
	}

	if (s_nodes.empty())
		return;

	size_t n = s_nodes.size();
	std::map<Grid::Node *, size_t> nodeToIndex;
	std::map<Grid::Node *, Core::f64> nodeZBase;

	for (size_t i = 0; i < n; ++i) {
		nodeToIndex[s_nodes[i]] = i;
		Core::f64 vbase = s_nodes[i]->getBaseVoltage();
		nodeZBase[s_nodes[i]] = (vbase * vbase) / (S_BASE / 1000.0);
	}

	std::map<std::pair<size_t, size_t>, std::complex<Core::f64>> y_temp;

	for (auto line : allLines) {
		auto from = line->getFromNode();
		auto to = line->getToNode();
		if (!from || !to)
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

		Core::f64 a =
			trafo->getTap() / (pri->getBaseVoltage() / sec->getBaseVoltage());
		if (std::abs(a) < 1e-6)
			a = 1.0;

		y_temp[{i, i}] += y / (a * a);
		y_temp[{j, j}] += y;
		y_temp[{i, j}] -= y / a;
		y_temp[{j, i}] -= y / a;
	}

	for (auto breaker : allBreakers) {
		if (breaker->isOpen())
			continue;

		auto from = breaker->getFromNode();
		auto to = breaker->getToNode();
		if (!from || !to)
			continue;

		size_t i = nodeToIndex[from];
		size_t j = nodeToIndex[to];

		std::complex<Core::f64> y(1e6, 0.0);

		y_temp[{i, j}] -= y;
		y_temp[{j, i}] -= y;
		y_temp[{i, i}] += y;
		y_temp[{j, j}] += y;
	}

	std::vector<std::complex<Core::f64>> values;
	std::vector<size_t> colIndex;
	std::vector<size_t> rowPtr;

	rowPtr.push_back(0);
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < n; ++j) {
			if (y_temp.count({i, j})) {
				values.push_back(y_temp[{i, j}]);
				colIndex.push_back(j);
			}
		}
		rowPtr.push_back(values.size());
	}

	s_yBus =
		std::make_unique<Util::SparseMatrix<std::complex<Core::f64>>>(n, n);
	s_yBus->setDirect(values, colIndex, rowPtr);
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
bool PowerSolver::runIteration() {
	if (!s_yBus)
		return true;
	size_t n = s_nodes.size();

	std::vector<size_t> pq_indices, pv_indices;
	size_t slack_idx = 0;

	for (size_t i = 0; i < n; ++i) {
		auto node = s_nodes[i];
		bool isSlack = (i == 0);
		bool isPV = false;
		for (auto gen : s_generators) {
			if (gen->getConnectedNode() == node) {
				if (gen->getMode() == Grid::GeneratorMode::Slack)
					isSlack = true;
				if (gen->getMode() == Grid::GeneratorMode::PV)
					isPV = true;
			}
		}
		if (isSlack)
			slack_idx = i;
		else if (isPV)
			pv_indices.push_back(i);
		else
			pq_indices.push_back(i);
	}

	std::vector<PowerMismatch> mismatches(n);

	for (auto gen : s_generators) {
		if (gen->getMode() == Grid::GeneratorMode::Slack) {
			size_t idx = 0;
			for (size_t i = 0; i < n; ++i)
				if (s_nodes[i] == gen->getConnectedNode()) {
					idx = i;
					break;
				}
			Core::f64 angle = std::arg(s_nodes[idx]->getVoltage());
			s_nodes[idx]->setVoltage(std::polar(gen->getTargetV(), angle));
		}
	}

	for (size_t i = 0; i < n; ++i) {
		std::complex<Core::f64> Vi_pu = s_nodes[i]->getVoltage();
		std::complex<Core::f64> Ii_pu(0, 0);

		const auto &vals = s_yBus->getValues();
		const auto &cols = s_yBus->getColIndex();
		const auto &rowPtr = s_yBus->getRowPtr();

		for (size_t k = rowPtr[i]; k < rowPtr[i + 1]; ++k)
			Ii_pu += vals[k] * s_nodes[cols[k]]->getVoltage();

		std::complex<Core::f64> Si_pu = Vi_pu * std::conj(Ii_pu);

		Core::f64 P_sched_pu = 0, Q_sched_pu = 0;
		for (auto gen : s_generators)
			if (gen->getConnectedNode() == s_nodes[i])
				P_sched_pu += gen->getActualP() / S_BASE;
		for (auto load : s_loads) {
			if (load->getConnectedNode() == s_nodes[i]) {
				Core::f64 P = load->getCurrentConsumption();
				Core::f64 pf = load->getPowerFactor();
				P_sched_pu -= P / S_BASE;
				Q_sched_pu -= (P * std::sqrt(1.0 - pf * pf) / pf) / S_BASE;
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

	if (maxMismatch < 1e-4)
		return true;

	size_t n_pq = pq_indices.size();
	size_t n_pv = pv_indices.size();
	size_t dim = n_pq * 2 + n_pv;

	std::vector<std::vector<Core::f64>> J(dim,
										  std::vector<Core::f64>(dim, 0.0));
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

		std::complex<Core::f64> Vi_pu = s_nodes[i]->getVoltage();
		Core::f64 Vi = std::abs(Vi_pu);
		Core::f64 delta_i = std::arg(Vi_pu);

		Core::f64 Pi = 0, Qi = 0;
		const auto &vals = s_yBus->getValues();
		const auto &cols = s_yBus->getColIndex();
		const auto &rowPtr = s_yBus->getRowPtr();

		for (size_t k = rowPtr[i]; k < rowPtr[i + 1]; ++k) {
			size_t j = cols[k];
			std::complex<Core::f64> Vj_pu = s_nodes[j]->getVoltage();
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

			Core::f64 vk_base = s_nodes[j]->getBaseVoltage();
			std::complex<Core::f64> Vj_pu = s_nodes[j]->getVoltage() / vk_base;
			Core::f64 Vj = std::abs(Vj_pu);
			Core::f64 delta_j = std::arg(Vj_pu);
			std::complex<Core::f64> Yij = vals[k];
			Core::f64 Gij = Yij.real();
			Core::f64 Bij = Yij.imag();
			Core::f64 dij = delta_i - delta_j;

			if (i == j) {
				J[row_theta][row_theta] = -Qi - Bij * Vi * Vi;
				if (v_map.count(i)) {
					size_t row_v = v_map[i];
					J[row_theta][row_v] = (Pi + Gij * Vi * Vi) / Vi;
					J[row_v][row_theta] = Pi - Gij * Vi * Vi;
					J[row_v][row_v] = (Qi - Bij * Vi * Vi) / Vi;
				}
			} else {
				size_t col_theta = theta_map[j];
				J[row_theta][col_theta] =
					Vi * Vj * (Gij * sin(dij) - Bij * cos(dij));
				if (v_map.count(i)) {
					size_t row_v = v_map[i];
					J[row_v][col_theta] =
						-Vi * Vj * (Gij * cos(dij) + Bij * sin(dij));
					if (v_map.count(j)) {
						size_t col_v = v_map[j];
						J[row_theta][col_v] =
							Vi * (Gij * cos(dij) + Bij * sin(dij));
						J[row_v][col_v] =
							Vi * (Gij * sin(dij) - Bij * cos(dij));
					}
				} else if (v_map.count(j)) {
					size_t col_v = v_map[j];
					J[row_theta][col_v] =
						Vi * (Gij * cos(dij) + Bij * sin(dij));
				}
			}
		}
	}

	std::vector<Core::f64> dx;
	simpleSolve(J, b, dx);

	for (size_t i : pq_indices) {
		Core::f64 angle = std::arg(s_nodes[i]->getVoltage()) + dx[theta_map[i]];
		Core::f64 mag_pu = std::abs(s_nodes[i]->getVoltage()) + dx[v_map[i]];
		mag_pu = std::clamp(mag_pu, 0.5, 1.5);
		s_nodes[i]->setVoltage(std::polar(mag_pu, angle));
	}
	for (size_t i : pv_indices) {
		Core::f64 angle = std::arg(s_nodes[i]->getVoltage()) + dx[theta_map[i]];
		Core::f64 mag_pu = std::abs(s_nodes[i]->getVoltage());
		s_nodes[i]->setVoltage(std::polar(mag_pu, angle));
	}

	return false;
}

} // namespace GLStation::Simulation
