#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"
#include <complex>

namespace GLStation::Grid {

class Line : public GridComponent {
public:
  Line(std::string name, Node *from, Node *to, Core::f64 r, Core::f64 x);

  void tick(Core::Tick currentTick) override;
  std::string toString() const override;

  std::complex<Core::f64> getAdmittance() const { return m_admittance; }
  Node *getFromNode() const { return m_from; }
  Node *getToNode() const { return m_to; }
  Core::f64 getResistance() const { return m_resistance; }
  Core::f64 getReactance() const { return m_reactance; }
  std::complex<Core::f64> getCurrentFlow() const { return m_currentFlow; }
  Core::f64 getCurrentLimit() const { return m_currentLimit; }
  void setCurrentLimit(Core::f64 limit) { m_currentLimit = limit; }
  Core::f64 getLosses() const {
    Core::f64 iMag = std::abs(m_currentFlow);
    return iMag * iMag * m_resistance / 1000.0;
  }

private:
  Node *m_from;
  Node *m_to;
  Core::f64 m_resistance;
  Core::f64 m_reactance;
  Core::f64 m_currentLimit;
  std::complex<Core::f64> m_admittance;
  std::complex<Core::f64> m_currentFlow;
};

} // namespace GLStation::Grid
