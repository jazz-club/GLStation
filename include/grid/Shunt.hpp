#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"

namespace GLStation::Grid {

class Shunt : public GridComponent {
  public:
	Shunt(std::string name, Node *node, Core::f64 gPu, Core::f64 bPu);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	Node *getNode() const { return m_node; }
	Core::f64 getGPu() const { return m_gPu; }
	Core::f64 getBPu() const { return m_bPu; }
	void setAdmittancePu(Core::f64 gPu, Core::f64 bPu);

  private:
	Node *m_node;
	Core::f64 m_gPu;
	Core::f64 m_bPu;
};

} // namespace GLStation::Grid
