#pragma once

#include "core/Types.hpp"
#include "grid/GridComponent.hpp"
#include <complex>

namespace GLStation::Grid {

class Node : public GridComponent {
  public:
	Node(std::string name, Core::f64 baseVoltage);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	void setVoltage(std::complex<Core::f64> voltage);
	std::complex<Core::f64> getVoltage() const { return m_voltage; }
	Core::f64 getBaseVoltage() const { return m_baseVoltage; }

  private:
	Core::f64 m_baseVoltage;
	std::complex<Core::f64> m_voltage;
};

} // namespace GLStation::Grid
