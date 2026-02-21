#pragma once

#include "grid/GridComponent.hpp"
#include <memory>
#include <vector>

namespace GLStation::Grid {

class Substation : public GridComponent {
  public:
	Substation(std::string name);

	void addComponent(std::shared_ptr<GridComponent> component);
	void tick(Core::Tick currentTick) override;
	std::string toString() const override;
	const std::vector<std::shared_ptr<GridComponent>> &getComponents() const {
		return m_components;
	}

  private:
	std::vector<std::shared_ptr<GridComponent>> m_components;
};

} // namespace GLStation::Grid
