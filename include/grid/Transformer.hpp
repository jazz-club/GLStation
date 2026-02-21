#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"
#include <complex>

namespace GLStation::Grid {

class Transformer : public GridComponent {
  public:
	Transformer(std::string name, Node *primary, Node *secondary, Core::f64 r,
				Core::f64 x, Core::f64 tap = 1.0);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	std::complex<Core::f64> getAdmittance() const;
	Core::f64 getTap() const { return m_tap; }
	Node *getPrimaryNode() const { return m_primary; }
	Node *getSecondaryNode() const { return m_secondary; }
	Core::f64 getCurrentLimit() const { return m_currentLimit; }
	void setCurrentLimit(Core::f64 limit) { m_currentLimit = limit; }
	void setTap(Core::f64 tap) { m_tap = tap; }
	Core::f64 getResistance() const { return m_resistance; }
	Core::f64 getReactance() const { return m_reactance; }
	Core::f64 getLosses() const;
	std::complex<Core::f64> getCurrentFlow() const { return m_currentFlow; }

  private:
	Node *m_primary;
	Node *m_secondary;
	Core::f64 m_resistance;
	Core::f64 m_reactance;
	Core::f64 m_tap;
	Core::f64 m_currentLimit;
	std::complex<Core::f64> m_admittance;
	std::complex<Core::f64> m_currentFlow;
};

} // namespace GLStation::Grid
