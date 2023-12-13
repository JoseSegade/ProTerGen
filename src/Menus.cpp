#include "Menus.h"
#if _DEBUG && PRINT_PERFORMANCE_TIMES
#include "Timer.h"
#include "TerrainTree.h"
#endif // DEBUG

#if _DEBUG && PRINT_PERFORMANCE_TIMES

static void RestartMetrics()
{
	using namespace ProTerGen;

	TerrainQTMorphSystem::MetricResetCountMean();
	Timer::RestartEvent((Timer::TimerEvent)Timer::Events::INDIRECTION_UPDATE);
	Timer::RestartEvent((Timer::TimerEvent)Timer::Events::TERRAIN_QT);
	Timer::RestartEvent((Timer::TimerEvent)Timer::Events::TERRAIN_MESH);
	Timer::RestartEvent((Timer::TimerEvent)Timer::Events::UPDATE_LOOP);
	Timer::RestartEvent((Timer::TimerEvent)Timer::Events::DRAW_LOOP);
	Timer::RestartEvent((Timer::TimerEvent)Timer::Events::MAIN_LOOP);
}

static void PrintMetrics()
{
	using namespace ProTerGen;
	const double pg = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::PAGE_GEN)          ;
	const double iu = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::INDIRECTION_UPDATE);
	const double tq = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::TERRAIN_QT)        ;
	const double tm = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::TERRAIN_MESH)      ;
	const double tv = TerrainQTMorphSystem::MetricGetVertexCountMean()                                     ;
	const double ul = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::UPDATE_LOOP)       ;
	const double dl = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::DRAW_LOOP)         ;
	const double ml = Timer::GetLastTimeRequestedTime((Timer::TimerEvent)Timer::Events::MAIN_LOOP)         ;
	printf("---------------------------------\n");
	printf("Page gen: %lf(ms)\n",           pg);
	printf("Indirection update: %lf(ms)\n", iu);
	printf("QuadTree gen: %lf(ms)\n",       tq);
	printf("Terrain mesh build: %lf(ms)\n", tm);
	printf("Num vertices terrain: %lf\n",   tv);
	printf("Update loop time: %lf(ms)\n",   ul);
	printf("Draw loop time: %lf(ms)\n",     dl);
	printf("Main loop time: %lf(ms)\n",     ml);
	printf("---------------------------------\n");
	RestartMetrics();
	/*
	const char* latexFormat = 
		"1 & 1 & 1 & "
		"%.3lf & %.3lf & %.3lf & %.3lf & "
		"%.0lf & %.3lf & %.3lf & %.3lf \\\\"
		"\n"
		;
	printf(latexFormat,
		pg,
		iu,
		tq,
		tm,
		tv,
		ul,
		dl,
		ul + dl
	);
	*/
}

#endif

void ProTerGen::MainMenuSystem::Init()
{
}

static void Shortcuts(ProTerGen::MainConfig& mainConfig)
{
	if (ImGui::IsKeyReleased(ImGuiKey_F1))
	{
		mainConfig.DebugVT = !mainConfig.DebugVT;
	}
	else if (ImGui::IsKeyReleased(ImGuiKey_F2))
	{
		mainConfig.UpdateVT = !mainConfig.UpdateVT;
	}
	else if (ImGui::IsKeyReleased(ImGuiKey_F3))
	{
		mainConfig.UpdateTerrain = !mainConfig.UpdateTerrain;
	}
	else if (ImGui::IsKeyReleased(ImGuiKey_F4))
	{
		mainConfig.DrawBorderColor = !mainConfig.DrawBorderColor;
	}
	else if (ImGui::IsKeyReleased(ImGuiKey_F5))
	{
		mainConfig.DebugTerrainLod = !mainConfig.DebugTerrainLod;
	}

#if _DEBUG && PRINT_PERFORMANCE_TIMES
	
	if (ImGui::IsKeyReleased(ImGuiKey_P))
	{
		PrintMetrics();
	}
	if (ImGui::IsKeyReleased(ImGuiKey_O))
	{
		RestartMetrics();
	}

#endif

}

void ProTerGen::MainMenuSystem::Update(double dt)
{
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_HorizontalScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_MenuBar;

	for (auto entity : mEntities)
	{
		MainConfig& mainConfig = mRegister->GetComponent<MainConfig>(entity);
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit", "ESC"))
				{
					mainConfig.OnExit();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Debug"))
			{
				ImGui::MenuItem("Show stats", NULL, &mainConfig.Metrics);
				ImGui::MenuItem("Texture Streaming Debug", "F1", &mainConfig.DebugVT);
				ImGui::MenuItem("Update Virtual Texture", "F2", &mainConfig.UpdateVT);
				ImGui::MenuItem("Update Terrain", "F3", &mainConfig.UpdateTerrain);
				ImGui::MenuItem("Draw Border Color", "F4", &mainConfig.DrawBorderColor);
				ImGui::MenuItem("Color Terrain By Lod", "F5", &mainConfig.DebugTerrainLod);
				if (ImGui::Button("Clear Virtual Texture Cache"))
				{
					mainConfig.ClearVTCache = true;
				}

#if _DEBUG && PRINT_PERFORMANCE_TIMES

				if (ImGui::Button("Print main loop time"))
				{
					PrintMetrics();
				}
				if (ImGui::Button("Restart loop metrics"))
				{
					RestartMetrics();
				}

#endif

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Config"))
			{
				ImGui::MenuItem("VSync Enabled", NULL, &mainConfig.VSync);
				ImGui::MenuItem("Wireframe", NULL, &mainConfig.Wireframe);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("Show Settings", NULL, &mShowCollections);
				ImGui::Separator();
				for (auto& [name, pair] : mCollections)
				{
					bool pressed = pair.second;
					ImGui::MenuItem(name.c_str(), NULL, &pair.second);
					if (!pressed && pair.second) mShowCollections = true;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		if (mainConfig.Metrics) ShowMetrics();
		BuildCollections();

		Shortcuts(mainConfig);
	}
}

void ProTerGen::MainMenuSystem::RegisterCollection(const std::string& name, std::vector<Entry>& entries, bool open)
{
	if (mCollections.count(name) < 1)
	{
		mCollections.emplace(name, std::make_pair(std::move(entries), open));
	}
	else
	{
		mCollections[name] = std::make_pair(std::move(entries), open);
	}
}

void ProTerGen::MainMenuSystem::ShowMetrics()
{
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetFrameHeight() + 10));
	ImGui::SetNextWindowSize(ImVec2((io.DisplaySize.x / 12) * 2 - 20, (io.DisplaySize.y / 12) * 1 - 20));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
	if (ImGui::Begin("Stats", NULL, windowFlags))
	{
		ImGui::Text("%.1f FPS \t %.3f ms", io.Framerate, io.DeltaTime * 1000.0f);
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void ProTerGen::MainMenuSystem::BuildCollections()
{
	if (!mShowCollections) return;
	ImGuiWindowFlags windowFlags = 
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysHorizontalScrollbar |
		ImGuiWindowFlags_NoSavedSettings;
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 400, ImGui::GetFrameHeight()));
	ImGui::SetNextWindowSize(ImVec2(400, io.DisplaySize.y - ImGui::GetFrameHeight()));
	if (ImGui::Begin("Settings", &mShowCollections, windowFlags))
	{
		ImGuiTabBarFlags tabBarFlags =
			ImGuiTabBarFlags_Reorderable |
			ImGuiTabBarFlags_TabListPopupButton |
			ImGuiTabBarFlags_FittingPolicyScroll;
		if (ImGui::BeginTabBar("TabBar", tabBarFlags))
		{
			for (auto& [name, pair] : mCollections)
			{
				auto& [entries, show] = pair;
				if (show && ImGui::BeginTabItem(name.c_str(), &show))
				{

					for (Entry& entry : entries)
					{
						ImGui::BeginDisabled(entry.disabled);
						switch (entry.type)
						{
						case EntryType::BOOL:
							ImGui::Checkbox(entry.name.c_str(), (bool*)entry.data);
							break;
						case EntryType::FLOAT:
							if (entry.slider)
							{
								ImGui::SliderFloat(entry.name.c_str(), (float*)entry.data, entry.min, entry.max, "%0.3f");
							}
							else
							{
								ImGui::InputFloat(entry.name.c_str(), (float*)entry.data);
							}
							break;
						case EntryType::UINT32:
							if (entry.slider)
							{
								ImGui::SliderInt(entry.name.c_str(), (int32_t*)entry.data, (int32_t)entry.min, (int32_t)entry.max);
							}
							else
							{
								ImGui::DragInt(entry.name.c_str(), (int32_t*)entry.data, 1.0f, (int32_t)entry.min, (int32_t)entry.max);
							}
							break;
						case EntryType::COLOR4:
							ImGui::ColorEdit4(entry.name.c_str(), (float*)entry.data, ImGuiColorEditFlags_Float);
							break;
						case EntryType::COLOR3:
							ImGui::ColorEdit3(entry.name.c_str(), (float*)entry.data, ImGuiColorEditFlags_Float);
							break;
						case EntryType::CUSTOM:
							((EntryFunctor*)(entry.data))->mFunc();
							break;
						case EntryType::FLOAT2:
							if (entry.slider)
							{
								ImGui::SliderFloat2(entry.name.c_str(), (float*)entry.data, entry.min, entry.max, "%0.3f");
							}
							else
							{
								ImGui::InputFloat2(entry.name.c_str(), (float*)entry.data);
							}
							break;
						case EntryType::FLOAT3:
							if (entry.slider)
							{
								ImGui::SliderFloat3(entry.name.c_str(), (float*)entry.data, entry.min, entry.max, "%0.3f");
							}
							else
							{
								ImGui::InputFloat3(entry.name.c_str(), (float*)entry.data);
							}
							break;
						default:
							ImGui::TextColored(ImVec4(1.0f, 0.475f, 0.0f, 1.0f), ("WARNING: Unknown variable {'" + entry.name + "'} display.").c_str());
							break;
						}
						ImGui::EndDisabled();
					}
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}
