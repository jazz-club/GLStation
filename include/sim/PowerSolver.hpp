#pragma once

#include "grid/Substation.hpp"
#include "sim/SparseMatrix.hpp"
#include "util/Types.hpp"
#include <complex>
#include <memory>
#include <vector>

namespace GLStation::Simulation {

struct SolverSettings {
	::GLStation::Core::f64 tolerance = 1e-5;
	::GLStation::Core::u32 maxIterations = 25;
};

class PowerSolver {
  public:
	static Core::f64 sBaseKw();
	static Core::f64 sBaseMva();
	static void
	solve(const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
			  &substations,
		  const SolverSettings &settings = SolverSettings());
	static void
	buildYBus(const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
				  &substations);
	static void invalidateYBus();

  private:
	static bool runIteration(const SolverSettings &settings);
	static void updateGeneratorQFromSolution();
	static void updateSlackGeneratorPowerFromSolution();
	static void syncBranchFlowsFromPUSolution();
	static bool resolvePvQLimits();
	static std::unique_ptr<Util::SparseMatrix<std::complex<Core::f64>>> s_yBus;
};

} // namespace GLStation::Simulation
