#pragma once

#include "grid/GridComponent.hpp"
#include "grid/Node.hpp"

namespace GLStation::Grid {

class Breaker : public GridComponent {
public:
  Breaker(std::string name, Node *from, Node *to);

  void tick(Core::Tick currentTick) override;
  std::string toString() const override;

  void setOpen(bool open) { m_isOpen = open; }
  bool isOpen() const { return m_isOpen; }
  Node *getFromNode() const { return m_from; }
  Node *getToNode() const { return m_to; }

private:
  Node *m_from;
  Node *m_to;
  bool m_isOpen;
};

} // namespace GLStation::Grid
