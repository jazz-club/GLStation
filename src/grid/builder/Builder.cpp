#include "grid/builder/Builder.hpp"
#include "grid/Substation.hpp"

namespace GLStation::Grid::Builder {

static std::shared_ptr<Substation> s_activeSubstation;

std::shared_ptr<Substation> BuilderShell::getActiveSubstation() {
	return s_activeSubstation;
}

void BuilderShell::setActiveSubstation(std::shared_ptr<Substation> sub) {
	s_activeSubstation = sub;
}

} // namespace GLStation::Grid::Builder
