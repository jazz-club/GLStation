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
	https://pathologic.fandom.com/wiki/Category:Locations
*/
void Engine::createDemoGrid() {
	auto s_earth_quarter =
		std::make_shared<Grid::Substation>("The_Earth_Quarter");
	auto s_the_works = std::make_shared<Grid::Substation>("The_Works");
	auto s_the_knots = std::make_shared<Grid::Substation>("The_Knots");
	auto s_stoneyard = std::make_shared<Grid::Substation>("The_Stoneyard");
	auto s_the_steppe = std::make_shared<Grid::Substation>("The_Steppe");

	auto n_factory_hub =
		std::make_shared<Grid::Node>("The_Works_Factory_Hub", 380.0);
	auto n_tanners = std::make_shared<Grid::Node>("The_Tanners_Slums", 110.0);

	auto n_the_gut = std::make_shared<Grid::Node>("The_Gut_Commercial", 33.0);
	auto n_hindquarters =
		std::make_shared<Grid::Node>("The_Hindquarters", 33.0);

	auto n_bridge_square =
		std::make_shared<Grid::Node>("Bridge_Square_Station", 380.0);
	auto n_town_marrow =
		std::make_shared<Grid::Node>("Town_Hall_Marrow", 110.0);
	auto n_theatre = std::make_shared<Grid::Node>("The_Theatre", 110.0);

	auto n_warehouses = std::make_shared<Grid::Node>("The_Warehouses", 33.0);
	auto n_the_ribs = std::make_shared<Grid::Node>("The_Ribs_Housing", 33.0);

	auto n_cathedral =
		std::make_shared<Grid::Node>("The_Cathedral_Center", 33.0);
	auto n_the_brain =
		std::make_shared<Grid::Node>("The_Brain_Residential", 33.0);

	auto n_steppe_terminal =
		std::make_shared<Grid::Node>("Steppe_Terminal_Hub", 380.0);
	auto n_polyhedron =
		std::make_shared<Grid::Node>("The_Polyhedron_Outlier", 110.0);
	auto n_abattoir = std::make_shared<Grid::Node>("The_Abattoir", 110.0);

	auto g_slack = std::make_shared<Grid::Generator>(
		"Town_Power_Station", n_factory_hub.get(), Grid::GeneratorMode::Slack);
	g_slack->setTargetV(1.02);
	g_slack->setPowerBounds(10000.0, 150000.0);

	auto g_crucible = std::make_shared<Grid::Generator>(
		"The_Crucible_Backup", n_town_marrow.get(), Grid::GeneratorMode::PV);
	g_crucible->setTargetP(30000.0);
	g_crucible->setTargetV(1.01);
	g_crucible->setPowerBounds(5000.0, 50000.0);

	auto g_steppe_wind = std::make_shared<Grid::Generator>(
		"Steppe_Wind_Array", n_polyhedron.get(), Grid::GeneratorMode::PV);
	g_steppe_wind->setTargetP(40000.0);
	g_steppe_wind->setTargetV(1.01);
	g_steppe_wind->setPowerBounds(0.0, 55000.0);

	auto l_factory = std::make_shared<Grid::Load>("Load_The_Works_Industrial",
												  n_factory_hub.get(), 40000.0);
	auto l_tanners = std::make_shared<Grid::Load>("Load_Tanners_Ghetto",
												  n_tanners.get(), 18000.0);
	auto l_gut = std::make_shared<Grid::Load>("Load_The_Gut_Shops",
											  n_the_gut.get(), 10000.0);
	auto l_hindquarters = std::make_shared<Grid::Load>(
		"Load_Hindquarters_Housing", n_hindquarters.get(), 12000.0);
	auto l_town_hall = std::make_shared<Grid::Load>(
		"Load_Town_Hall", n_town_marrow.get(), 7000.0);
	auto l_theatre = std::make_shared<Grid::Load>("Load_Theatre_District",
												  n_theatre.get(), 9000.0);
	auto l_warehouses = std::make_shared<Grid::Load>(
		"Load_Warehouses_Docks", n_warehouses.get(), 15000.0);
	auto l_ribs = std::make_shared<Grid::Load>("Load_The_Ribs_Slums",
											   n_the_ribs.get(), 20000.0);
	auto l_cathedral = std::make_shared<Grid::Load>("Load_The_Cathedral",
													n_cathedral.get(), 5000.0);
	auto l_brain = std::make_shared<Grid::Load>("Load_The_Brain_Elite",
												n_the_brain.get(), 8000.0);
	auto l_abattoir = std::make_shared<Grid::Load>("Load_The_Abattoir",
												   n_abattoir.get(), 12000.0);

	auto t_earth = std::make_shared<Grid::Transformer>(
		"T_Earth_380_110", n_factory_hub.get(), n_tanners.get(), 0.02, 0.1,
		1.0);
	auto t_knots = std::make_shared<Grid::Transformer>(
		"T_Knots_380_110", n_bridge_square.get(), n_town_marrow.get(), 0.02,
		0.1, 1.0);
	auto t_stoneyard = std::make_shared<Grid::Transformer>(
		"T_Stone_380_110", n_steppe_terminal.get(), n_polyhedron.get(), 0.02,
		0.1, 1.0);

	auto t_gut = std::make_shared<Grid::Transformer>(
		"T_Gut_110_33", n_tanners.get(), n_the_gut.get(), 0.01, 0.05, 1.0);
	auto t_warehouse = std::make_shared<Grid::Transformer>(
		"T_Whouse_110_33", n_town_marrow.get(), n_warehouses.get(), 0.01, 0.05,
		1.0);
	auto t_cathedral =
		std::make_shared<Grid::Transformer>("T_Cath_110_33", n_polyhedron.get(),
											n_cathedral.get(), 0.01, 0.05, 1.0);

	auto line_guzzle = std::make_shared<Grid::Line>(
		"The_Guzzle_Power_Crossing", n_factory_hub.get(), n_bridge_square.get(),
		0.01, 0.05);
	auto line_gullet = std::make_shared<Grid::Line>(
		"The_Gullet_Power_Crossing", n_bridge_square.get(),
		n_steppe_terminal.get(), 0.01, 0.06);
	auto r_steppe_back = std::make_shared<Grid::Line>(
		"Steppe_Perimeter_Line", n_steppe_terminal.get(), n_factory_hub.get(),
		0.01, 0.07);
	line_guzzle->setCurrentLimit(2500.0);
	line_gullet->setCurrentLimit(2500.0);
	r_steppe_back->setCurrentLimit(2500.0);

	auto l_tanner_tie =
		std::make_shared<Grid::Line>("Tanner_Hindquarters_Tie", n_the_gut.get(),
									 n_hindquarters.get(), 0.04, 0.15);
	auto l_town_tie = std::make_shared<Grid::Line>(
		"The_Theatre_Link", n_town_marrow.get(), n_theatre.get(), 0.03, 0.12);
	auto l_warehouse_ribs =
		std::make_shared<Grid::Line>("Warehouse_Ribs_Circuit", n_the_gut.get(),
									 n_the_ribs.get(), 0.03, 0.13);
	auto l_stone_tie =
		std::make_shared<Grid::Line>("Cathedral_Brain_Tie", n_cathedral.get(),
									 n_the_brain.get(), 0.05, 0.18);
	auto l_abattoir_tie = std::make_shared<Grid::Line>(
		"Abattoir_Feed", n_polyhedron.get(), n_abattoir.get(), 0.04, 0.15);

	l_tanner_tie->setCurrentLimit(200.0);

	s_the_works->addComponent(n_factory_hub);
	s_the_works->addComponent(g_slack);
	s_the_works->addComponent(l_factory);

	s_earth_quarter->addComponent(n_tanners);
	s_earth_quarter->addComponent(n_the_gut);
	s_earth_quarter->addComponent(n_hindquarters);
	s_earth_quarter->addComponent(l_tanners);
	s_earth_quarter->addComponent(l_gut);
	s_earth_quarter->addComponent(l_hindquarters);
	s_earth_quarter->addComponent(t_earth);
	s_earth_quarter->addComponent(t_gut);
	s_earth_quarter->addComponent(l_tanner_tie);

	s_the_knots->addComponent(n_bridge_square);
	s_the_knots->addComponent(n_town_marrow);
	s_the_knots->addComponent(n_theatre);
	s_the_knots->addComponent(n_warehouses);
	s_the_knots->addComponent(n_the_ribs);
	s_the_knots->addComponent(g_crucible);
	s_the_knots->addComponent(l_town_hall);
	s_the_knots->addComponent(l_theatre);
	s_the_knots->addComponent(l_warehouses);
	s_the_knots->addComponent(l_ribs);
	s_the_knots->addComponent(t_knots);
	s_the_knots->addComponent(t_warehouse);
	s_the_knots->addComponent(l_town_tie);
	s_the_knots->addComponent(l_warehouse_ribs);
	s_the_knots->addComponent(line_guzzle);

	s_stoneyard->addComponent(n_cathedral);
	s_stoneyard->addComponent(n_the_brain);
	s_stoneyard->addComponent(l_cathedral);
	s_stoneyard->addComponent(l_brain);
	s_stoneyard->addComponent(t_cathedral);
	s_stoneyard->addComponent(l_stone_tie);
	s_stoneyard->addComponent(line_gullet);

	s_the_steppe->addComponent(n_steppe_terminal);
	s_the_steppe->addComponent(n_polyhedron);
	s_the_steppe->addComponent(n_abattoir);
	s_the_steppe->addComponent(g_steppe_wind);
	s_the_steppe->addComponent(l_abattoir);
	s_the_steppe->addComponent(t_stoneyard);
	s_the_steppe->addComponent(r_steppe_back);
	s_the_steppe->addComponent(l_abattoir_tie);

	m_substations.push_back(s_the_works);
	m_substations.push_back(s_earth_quarter);
	m_substations.push_back(s_the_knots);
	m_substations.push_back(s_stoneyard);
	m_substations.push_back(s_the_steppe);

	logEvent("Demo grid loaded."
			 "X nodes");
}

void Engine::configureDemoProfiles() {
	for (auto &sub : m_substations) {
		for (auto &comp : sub->getComponents()) {
			if (auto load = dynamic_cast<Grid::Load *>(comp.get())) {
				const std::string n = load->getName();
				if (n.find("Housing") != std::string::npos ||
					n.find("Ghetto") != std::string::npos ||
					n.find("Slums") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Residential);
				else if (n.find("Shops") != std::string::npos ||
						 n.find("Hall") != std::string::npos ||
						 n.find("Theatre") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Commercial);
				else if (n.find("Industrial") != std::string::npos ||
						 n.find("Warehouses") != std::string::npos ||
						 n.find("Abattoir") != std::string::npos)
					load->setProfile(Grid::LoadProfile::Industrial);
				else
					load->setProfile(Grid::LoadProfile::Flat);
			}
			if (auto gen = dynamic_cast<Grid::Generator *>(comp.get())) {
				const std::string n = gen->getName();
				if (n.find("Wind") != std::string::npos)
					gen->setProfile(Grid::GeneratorProfile::Wind);
				else
					gen->setProfile(Grid::GeneratorProfile::Thermal);
			}
		}
	}
}

} // namespace GLStation::Simulation
