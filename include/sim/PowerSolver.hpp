#pragma once

#include "core/Types.hpp"
#include "grid/Substation.hpp"
#include "sim/SparseMatrix.hpp"
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
	static void
	solve(const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
			  &substations,
		  const SolverSettings &settings = SolverSettings());
	static void
	buildYBus(const std::vector<std::shared_ptr<::GLStation::Grid::Substation>>
				  &substations);
	static void invalidateYBus();

  private:
	static bool runIteration();
	static std::unique_ptr<Util::SparseMatrix<std::complex<Core::f64>>> s_yBus;
};

} // namespace GLStation::Simulation
