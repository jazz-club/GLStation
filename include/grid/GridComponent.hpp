#pragma once

#include "util/Types.hpp"
#include <string>

namespace GLStation::Grid {

class GridComponent {
  public:
	GridComponent(std::string name);
	virtual ~GridComponent() = default;

	virtual void tick(Core::Tick currentTick) = 0;
	virtual std::string toString() const;

	const std::string &getName() const { return m_name; }
	Core::u64 getId() const { return m_id; }
	static void resetIdCounter();

  protected:
	Core::u64 m_id;
	std::string m_name;

  private:
	static Core::u64 s_nextId;
};

} // namespace GLStation::Grid
