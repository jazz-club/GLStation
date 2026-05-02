#include "grid/Breaker.hpp"
#include "grid/Generator.hpp"
#include "grid/GridComponent.hpp"
#include "grid/Line.hpp"
#include "grid/Load.hpp"
#include "grid/Node.hpp"
#include "grid/Shunt.hpp"
#include "grid/Transformer.hpp"
#include "sim/Engine.hpp"
#include <iostream>
#include <memory>
#include <vector>

namespace GLStation::Simulation {

/*
	https://www.nationalgridet.com/infrastructure-projects/london-power-tunnels
	https://www.murphygroup.com/project/london-power-tunnels-phase-2/
	https://www.murphygroup.com/project/london-power-tunnel-highbury-132kv/
*/

void Engine::createDemoGrid() {
	auto s_central = std::make_shared<Grid::Substation>("Central_London");
	auto s_north = std::make_shared<Grid::Substation>("North_London");
	auto s_east = std::make_shared<Grid::Substation>("East_London");
	auto s_south = std::make_shared<Grid::Substation>("South_London");
	auto s_west = std::make_shared<Grid::Substation>("West_London");

	auto n_central_400 =
		std::make_shared<Grid::Node>("Westminster_400kV", 400.0);
	auto n_central_132 =
		std::make_shared<Grid::Node>("Westminster_132kV", 132.0);

	auto n_north_400 = std::make_shared<Grid::Node>("Camden_400kV", 400.0);
	auto n_north_132 = std::make_shared<Grid::Node>("Camden_132kV", 132.0);

	auto n_east_400 = std::make_shared<Grid::Node>("Newham_400kV", 400.0);
	auto n_east_132 = std::make_shared<Grid::Node>("Newham_132kV", 132.0);

	auto n_south_400 = std::make_shared<Grid::Node>("Southwark_400kV", 400.0);
	auto n_south_132 = std::make_shared<Grid::Node>("Southwark_132kV", 132.0);

	auto n_west_400 = std::make_shared<Grid::Node>("Kensington_400kV", 400.0);
	auto n_west_132 = std::make_shared<Grid::Node>("Kensington_132kV", 132.0);

	auto g_national_north = std::make_shared<Grid::Generator>(
		"National_Grid_Elstree", n_north_400.get(), Grid::GeneratorMode::Slack);
	g_national_north->setTargetV(1.02);
	g_national_north->setPowerBounds(0.0, 5000000.0);

	auto g_belvedere = std::make_shared<Grid::Generator>(
		"Belvedere_Incinerator", n_east_132.get(), Grid::GeneratorMode::PV);
	g_belvedere->setTargetP(70000.0);
	g_belvedere->setTargetV(1.01);
	g_belvedere->setPowerBounds(0.0, 80000.0);

	auto g_national_west = std::make_shared<Grid::Generator>(
		"National_Grid_Iver", n_west_400.get(), Grid::GeneratorMode::PV);
	g_national_west->setTargetP(1500000.0);
	g_national_west->setTargetV(1.01);
	g_national_west->setPowerBounds(0.0, 3000000.0);

	auto l_central_commercial = std::make_shared<Grid::Load>(
		"Westminster_Commercial", n_central_132.get(), 800000.0);
	auto l_central_residential = std::make_shared<Grid::Load>(
		"Westminster_Housing", n_central_132.get(), 400000.0);

	auto l_north_residential = std::make_shared<Grid::Load>(
		"Camden_Housing", n_north_132.get(), 500000.0);
	auto l_north_commercial = std::make_shared<Grid::Load>(
		"Camden_Shops", n_north_132.get(), 200000.0);

	auto l_east_industrial = std::make_shared<Grid::Load>(
		"Newham_Industrial", n_east_132.get(), 600000.0);
	auto l_east_residential = std::make_shared<Grid::Load>(
		"Newham_Housing", n_east_132.get(), 400000.0);

	auto l_south_residential = std::make_shared<Grid::Load>(
		"Southwark_Housing", n_south_132.get(), 550000.0);
	auto l_south_commercial = std::make_shared<Grid::Load>(
		"Southwark_Commercial", n_south_132.get(), 250000.0);

	auto l_west_residential = std::make_shared<Grid::Load>(
		"Kensington_Housing", n_west_132.get(), 450000.0);
	auto l_west_commercial = std::make_shared<Grid::Load>(
		"Kensington_Shops", n_west_132.get(), 350000.0);

	auto t_central = std::make_shared<Grid::Transformer>(
		"T_Central_400_132", n_central_400.get(), n_central_132.get(), 0.01,
		0.1, 1.0);
	auto t_north = std::make_shared<Grid::Transformer>(
		"T_North_400_132", n_north_400.get(), n_north_132.get(), 0.01, 0.1,
		1.0);
	auto t_east = std::make_shared<Grid::Transformer>(
		"T_East_400_132", n_east_400.get(), n_east_132.get(), 0.01, 0.1, 1.0);
	auto t_south = std::make_shared<Grid::Transformer>(
		"T_South_400_132", n_south_400.get(), n_south_132.get(), 0.01, 0.1,
		1.0);
	auto t_west = std::make_shared<Grid::Transformer>(
		"T_West_400_132", n_west_400.get(), n_west_132.get(), 0.01, 0.1, 1.0);

	auto line_n_e = std::make_shared<Grid::Line>(
		"London_Ring_NE", n_north_400.get(), n_east_400.get(), 0.005, 0.05);
	auto line_e_s = std::make_shared<Grid::Line>(
		"London_Ring_ES", n_east_400.get(), n_south_400.get(), 0.005, 0.05);
	auto line_s_w = std::make_shared<Grid::Line>(
		"London_Ring_SW", n_south_400.get(), n_west_400.get(), 0.005, 0.05);
	auto line_w_n = std::make_shared<Grid::Line>(
		"London_Ring_WN", n_west_400.get(), n_north_400.get(), 0.005, 0.05);

	auto tie_n_c =
		std::make_shared<Grid::Line>("Tie_North_Central", n_north_400.get(),
									 n_central_400.get(), 0.002, 0.02);
	auto tie_s_c =
		std::make_shared<Grid::Line>("Tie_South_Central", n_south_400.get(),
									 n_central_400.get(), 0.002, 0.02);
	auto tie_w_c = std::make_shared<Grid::Line>(
		"Tie_West_Central", n_west_400.get(), n_central_400.get(), 0.002, 0.02);

	line_n_e->setCurrentLimit(5000.0);
	line_e_s->setCurrentLimit(5000.0);
	line_s_w->setCurrentLimit(5000.0);
	line_w_n->setCurrentLimit(5000.0);
	tie_n_c->setCurrentLimit(4000.0);
	tie_s_c->setCurrentLimit(4000.0);
	tie_w_c->setCurrentLimit(4000.0);

	s_central->addComponent(n_central_400);
	s_central->addComponent(n_central_132);
	s_central->addComponent(l_central_commercial);
	s_central->addComponent(l_central_residential);
	s_central->addComponent(t_central);

	s_north->addComponent(n_north_400);
	s_north->addComponent(n_north_132);
	s_north->addComponent(g_national_north);
	s_north->addComponent(l_north_residential);
	s_north->addComponent(l_north_commercial);
	s_north->addComponent(t_north);
	s_north->addComponent(tie_n_c);
	s_north->addComponent(line_n_e);

	s_east->addComponent(n_east_400);
	s_east->addComponent(n_east_132);
	s_east->addComponent(g_belvedere);
	s_east->addComponent(l_east_industrial);
	s_east->addComponent(l_east_residential);
	s_east->addComponent(t_east);
	s_east->addComponent(line_e_s);

	s_south->addComponent(n_south_400);
	s_south->addComponent(n_south_132);
	s_south->addComponent(l_south_residential);
	s_south->addComponent(l_south_commercial);
	s_south->addComponent(t_south);
	s_south->addComponent(line_s_w);
	s_south->addComponent(tie_s_c);

	s_west->addComponent(n_west_400);
	s_west->addComponent(n_west_132);
	s_west->addComponent(g_national_west);
	s_west->addComponent(l_west_residential);
	s_west->addComponent(l_west_commercial);
	s_west->addComponent(t_west);
	s_west->addComponent(line_w_n);
	s_west->addComponent(tie_w_c);

	m_substations.push_back(s_central);
	m_substations.push_back(s_north);
	m_substations.push_back(s_east);
	m_substations.push_back(s_south);
	m_substations.push_back(s_west);

	logEvent("GET IN!");
}

void Engine::configureDemoProfiles() {
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				const std::string n = load->getName();
				if (n.find("Housing") != std::string::npos ||
					n.find("Residential") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Residential);
				else if (n.find("Commercial") != std::string::npos ||
						 n.find("Shops") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Commercial);
				else if (n.find("Industrial") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Industrial);
				else
					load->setProfile(Grid::LoadProfile::Flat);
			}
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				const std::string n = gen->getName();
				if (n.find("Incinerator") != std::string::npos)
					gen->setProfile(Grid::GeneratorProfile::Thermal);
				else
					gen->setProfile(Grid::GeneratorProfile::Manual);
			}
		}
	}
}

} // namespace GLStation::Simulation
