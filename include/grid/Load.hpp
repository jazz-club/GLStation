#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"

namespace GLStation::Grid {

enum class LoadProfile { Flat, Residential, Commercial, Industrial };

class Load : public GridComponent {
  public:
	Load(std::string name, Node *connectedNode, Core::f64 maxPowerKw);

	void tick(Core::Tick currentTick) override;
	std::string toString() const override;

	Core::f64 getCurrentConsumption() const { return m_currentPowerKw; }
	Core::f64 getPowerFactor() const { return m_powerFactor; }
	void setPowerFactor(Core::f64 pf) { m_powerFactor = pf; }

	void setMaxPower(Core::f64 pKw) { m_maxPowerKw = pKw; }
	Core::f64 getMaxPower() const { return m_maxPowerKw; }
	void setProfile(LoadProfile profile) { m_profile = profile; }
	LoadProfile getProfile() const { return m_profile; }
	void setProfileStrength(Core::f64 strength) {
		m_profileStrength = strength;
	}

	void shed() { m_isShed = true; }
	void restore() { m_isShed = false; }
	bool isShed() const { return m_isShed; }

	Node *getConnectedNode() const { return m_connectedNode; }

  private:
	Node *m_connectedNode;
	Core::f64 m_maxPowerKw;
	Core::f64 m_currentPowerKw;
	Core::f64 m_powerFactor;
	bool m_isShed;
	LoadProfile m_profile;
	Core::f64 m_profileStrength;
};

} // namespace GLStation::Grid
