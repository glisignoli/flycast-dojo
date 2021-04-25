#include "DojoSession.hpp"
#include <oslib/audiostream.h>

DojoSession::DojoSession()
{
	Init();
}

void DojoSession::Init()
{
	// set defaults before attempting config load
	enabled = false;
	hosting = false;
	host_ip = "127.0.0.1";
	host_port = 7777;
	delay = 1;

	if (config::Training)
		player = 0;
	else
		player = config::DojoActAsServer ? 0 : 1;

	opponent = player == 0 ? 1 : 0;

	session_started = false;

	FrameNumber = 2;
	isPaused = true;

	MaxPlayers = 2;

	isMatchReady = false;
	isMatchStarted = false;

	UDPClient client;
	DojoLobby presence;

	client_input_authority = true;
	last_consecutive_common_frame = 2;
	started = false;

	spectating = false;
	transmitting = false;
	receiving = false;

	net_coin_press = false;

	replay_filename = "";

	host_status = 0;//"IDLE";

	OpponentIP = "";
	OpponentPing = 0;

	PlayMatch = false;
	ReplayFilename = "";

	lobby_active = false;
	disconnect_toggle = false;

	receiver_started = false;
	receiver_ended = false;

	transmitter_started = false;
	transmitter_ended = false;

	transmission_frames.clear();

	frame_timeout = 0;
	last_received_frame = 0;

	remaining_spectators = 0;

	MatchCode = "";
	SkipFrame = 746;
	DcSkipFrame = 180;

	recording = false;
	playing_input = false;
	playback_loop = false;
	record_player = player;

	if (!net_inputs[0].empty())
	{
		net_inputs[0].clear();
		net_input_keys[0].clear();

		net_inputs[1].clear();
		net_input_keys[1].clear();
	}

}

uint64_t DojoSession::DetectDelay(const char* ipAddr)
{
	uint64_t avg_ping_ms = client.GetOpponentAvgPing();

	int delay = (int)ceil(((int)avg_ping_ms * 1.0f) / 32.0f);
	config::Delay = delay > 1 ? delay : 1;

	return avg_ping_ms;
}

uint64_t DojoSession::unix_timestamp()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(); 
}

int DojoSession::PayloadSize()
{
	if (config::EnableBackfill)
		return num_back_frames * INPUT_SIZE + FRAME_SIZE;
	else
		return FRAME_SIZE;
}

int DojoSession::GetPlayer(u8* data)
{
	return (int)data[0];
}

int DojoSession::GetDelay(u8* data)
{
	return (int)data[1];
}

u32 DojoSession::GetFrameNumber(u8* data)
{
	return (int)(*(u32*)(data + 2));
}

u32 DojoSession::GetEffectiveFrameNumber(u8* data)
{
	return GetFrameNumber(data) + GetDelay(data);
}

void DojoSession::AddNetFrame(const char* received_data)
{
	const char data[FRAME_SIZE] = { 0 };
	memcpy((void*)data, received_data, FRAME_SIZE);

	u32 effective_frame_num = GetEffectiveFrameNumber((u8*)data);
	if (effective_frame_num == 0)
		return;

	u32 frame_player = data[0];
	u32 frame_player_opponent = frame_player == 0 ? 1 : 0;

	std::string data_to_queue(data, data + FRAME_SIZE);

	if (net_inputs[frame_player].count(effective_frame_num) == 0)
	{
		net_inputs[frame_player].insert(
			std::pair<u32, std::string>(effective_frame_num, data_to_queue));
		net_input_keys[frame_player].insert(effective_frame_num);
	}

	if (net_inputs[frame_player].count(effective_frame_num) == 1 &&
		net_inputs[frame_player_opponent].count(effective_frame_num) == 1)
	{
		if (effective_frame_num == last_consecutive_common_frame + 1)
			last_consecutive_common_frame++;
	}
}

std::string DojoSession::CreateFrame(unsigned int frame_num, int player, int delay, const char* input)
{
	unsigned char new_frame[FRAME_SIZE] = { 0 };
	new_frame[0] = player;
	new_frame[1] = delay;

	// enter current frame count in next 4 bytes
	memcpy(new_frame + 2, (unsigned char*)&frame_num, sizeof(unsigned int));

	if (input != 0)
		memcpy(new_frame + 6, (const char*)input, INPUT_SIZE);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		// dc analog starting position
		new_frame[10] = -128;
		new_frame[11] = -128;
	}

	char ret_frame[FRAME_SIZE] = { 0 };
	memcpy((void*)ret_frame, new_frame, FRAME_SIZE);

	std::string frame_str(ret_frame, ret_frame + FRAME_SIZE);

	return frame_str;
}

void DojoSession::AddBackFrames(const char* initial_frame, const char* back_inputs, int back_inputs_size)
{
	if (back_inputs_size == 0)
		return;

	int initial_player = GetPlayer((u8*)initial_frame);
	int initial_delay = GetDelay((u8*)initial_frame);
	u32 initial_frame_num = GetEffectiveFrameNumber((u8*)initial_frame);

	int all_inputs_size = back_inputs_size + INPUT_SIZE;
	char* all_inputs = new char[all_inputs_size];
	memcpy(all_inputs, initial_frame + 6, INPUT_SIZE);
	memcpy(all_inputs + INPUT_SIZE, back_inputs, back_inputs_size);

	// generates and adds missing frames from old inputs at the end of the payload
	for (int i = 0; i < ((back_inputs_size + INPUT_SIZE) / INPUT_SIZE); i++)
	{
		if (((int)initial_frame_num - i) > 2 &&
			net_inputs[initial_player].count(initial_frame_num - i) == 0)
		{
			char frame_fill[FRAME_SIZE] = { 0 };
			std::string new_frame = 
				CreateFrame(initial_frame_num - initial_delay - i, initial_player, 
					initial_delay, all_inputs + (i * INPUT_SIZE));
			memcpy((void*)frame_fill, new_frame.data(), FRAME_SIZE);
			AddNetFrame(frame_fill);

			if (config::Debug == DEBUG_BACKFILL ||
				config::Debug == DEBUG_APPLY_BACKFILL ||
				config::Debug == DEBUG_APPLY_BACKFILL_RECV ||
				config::Debug == DEBUG_ALL)
			{
				PrintFrameData("Backfilled", (u8 *)frame_fill);
			}
		}
	}

	delete[] all_inputs;
}

void DojoSession::pause()
{
	isPaused = true;
}

void DojoSession::resume()
{
	isPaused = false;
}

void DojoSession::StartSession(int session_delay, int session_ppf, int session_num_bf)
{
	if (config::RecordMatches && !dojo.PlayMatch)
		CreateReplayFile();

	FillDelay(session_delay);
	delay = session_delay;
	packets_per_frame = session_ppf;
	num_back_frames = session_num_bf;

	if (hosting)
		client.StartSession();

	session_started = true;
	isMatchStarted = true;

	dojo.resume();

	INFO_LOG(NETWORK, "Session Initiated");
}

void DojoSession::FillDelay(int fill_delay)
{
	for (int j = 0; j < MaxPlayers; j++)
	{
		net_inputs[j][0] = CreateFrame(0, j, 0, 0);
		net_inputs[j][1] = CreateFrame(1, j, 0, 0);
		net_inputs[j][2] = CreateFrame(1, j, 1, 0);

		for (int i = 1; i <= fill_delay; i++)
		{
			std::string new_frame = CreateFrame(2, j, i, 0);
			int new_index = GetEffectiveFrameNumber((u8*)new_frame.data());
			net_inputs[j][new_index] = new_frame;
			net_input_keys[j].insert(new_index);

			if (config::RecordMatches && !dojo.PlayMatch)
			{
				if (FrameNumber >= SkipFrame ||
					settings.platform.system != DC_PLATFORM_NAOMI)
				{
					AppendToReplayFile(new_frame);
				}
			}

			if (transmitter_started)
				dojo.transmission_frames.push_back(new_frame);
		}
	}
}

void DojoSession::FillSkippedFrames(u32 end_frame)
{
	u32 start_frame = net_inputs[0].size() - 1;

	for (int j = 0; j < MaxPlayers; j++)
	{
		for (int i = start_frame; i < end_frame; i++)
		{
			std::string new_frame = CreateFrame(i, j, delay, 0);
			int new_index = GetEffectiveFrameNumber((u8*)new_frame.data());
			net_inputs[j][new_index] = new_frame;
			net_input_keys[j].insert(new_index);
		}
	}
}

void DojoSession::StartTransmitterThread()
{
	if (transmitter_started)
		return;

	if (config::Transmitting)
	{
		std::thread t4(&DojoSession::transmitter_thread, std::ref(dojo));
		t4.detach();

		transmitter_started = true;
	}
}

int DojoSession::StartDojoSession()
{
	net_save_path = get_savestate_file_path(false);
	net_save_path.append(".net");

	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		SkipFrame = DcSkipFrame;

	if (receiving)
		config::Receiving = true;

	if (config::Receiving)
		receiving = true;

	if (dojo.PlayMatch)
	{
		if (config::TransmitReplays)
			StartTransmitterThread();
		FillDelay(1);
		LoadReplayFile(dojo.ReplayFilename);
		//resume();
	}
	else if (config::Receiving)
	{
		dojo.last_consecutive_common_frame = 2;
		dojo.FrameNumber = 2;
		dojo.FillDelay(2);

		if (!receiver_started)
		{
			std::thread t5(&DojoSession::receiver_thread, std::ref(dojo));
			t5.detach();

			receiver_started = true;
		}
	}
	else
	{
		LoadNetConfig();

		try
		{
			std::thread t2(&UDPClient::ClientThread, std::ref(client));
			t2.detach();

			if (config::EnableLobby && hosting)
			{
				std::thread t3(&DojoLobby::BeaconThread, std::ref(presence));
				t3.detach();
			}
		}
		catch (std::exception&)
		{
			return 1;
		}
	}
	return 0;
}

u16 DojoSession::ApplyNetInputs(PlainJoystickState* pjs, u32 port)
{
	return ApplyNetInputs(pjs, 0, port);
}

u16 DojoSession::ApplyNetInputs(u16 buttons, u32 port)
{
	return ApplyNetInputs(0, buttons, port);
}

// intercepts local input, assigns delay, stores in map
// applies stored input based on current frame count and player
u16 DojoSession::ApplyNetInputs(PlainJoystickState* pjs, u16 buttons, u32 port)
{
	InputPort = port;

	if (FrameNumber == 2)
	{
		if (dojo.PlayMatch)
		{
			resume();
		}
		else
		{
			std::ostringstream NoticeStream;
			if (hosting && !config::Receiving)
			{
				NoticeStream << "Hosting game on port " << host_port << " @ Delay " << delay;

				gui_display_notification(NoticeStream.str().data(), 9000);
				host_status = 3;// Hosting, Playing
			}
			else if (config::Receiving)
			{
				NoticeStream << "Listening to game stream on port " << config::SpectatorPort.get();
				gui_display_notification(NoticeStream.str().data(), 9000);
			}
			else
			{
				NoticeStream << "Connected to " << host_ip.data() << ":" << host_port << " @ Delay " << delay;
				gui_display_notification(NoticeStream.str().data(), 9000);
				host_status = 5;// Guest, Playing
			}
		}
	}


	if (FrameNumber == 3)
	{
		std::string net_save_path = get_savestate_file_path(false);
		net_save_path.append(".net");
		if (dojo.net_save_present && ghc::filesystem::exists(net_save_path))
		{
			jump_state_requested = true;
		}
	}

	if (config::Receiving &&
		receiver_ended &&
		disconnect_toggle)
	{
		gui_state = GuiState::EndSpectate;
	}

	while (isPaused && !disconnect_toggle);

	// advance game state
	if (port == 0)
	{
		FrameNumber++;

		if (config::Receiving &&
			receiver_ended &&
			FrameNumber > last_received_frame)
		{
			disconnect_toggle = true;
		}

		if (!spectating && !dojo.PlayMatch)
		{
			// if netplay savestate does not exist for both players
			// prevent input up to skipped frame, and save common state on both sides
			if (!ghc::filesystem::exists(net_save_path) || !dojo.net_save_present)
			{
				if (FrameNumber < SkipFrame)
				{
					if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
						settings.platform.system == DC_PLATFORM_ATOMISWAVE)
					{
						PlainJoystickState blank_pjs;
						CaptureAndSendLocalFrame(&blank_pjs);
					}
					else if (settings.platform.system == DC_PLATFORM_NAOMI)
					{
						CaptureAndSendLocalFrame((u16)0);
					}
				}

				if (FrameNumber == SkipFrame)
				{
					dc_savestate(net_save_path);
				}
			}

			// accept all inputs after skipped frame on first load
			// or after common save state is loaded
			if (FrameNumber >= SkipFrame ||
				(ghc::filesystem::exists(net_save_path) && dojo.net_save_present))
			{
				if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
					settings.platform.system == DC_PLATFORM_ATOMISWAVE)
				{
					CaptureAndSendLocalFrame(pjs);
				}
				else if (settings.platform.system == DC_PLATFORM_NAOMI)
				{
					CaptureAndSendLocalFrame(buttons);
				}
			}
		}
	}

	// be sure not to duplicate input directed to other ports
	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		PlainJoystickState blank_pjs;
		memcpy(pjs, &blank_pjs, sizeof(blank_pjs));
	}
	else
		buttons = 0;

	// assign inputs
	// inputs captured and synced in client thread
	std::string this_frame = "";

	if (dojo.PlayMatch &&
		(FrameNumber >= net_input_keys[0].size() ||
			FrameNumber >= net_input_keys[1].size()))
	{
		if (config::Transmitting)
			transmitter_ended = true;

		gui_state = GuiState::EndReplay;
		config::AutoSkipFrame = 1;
		dc_stop();
	}
/*
	if (config::Receiving &&
		dojo.receiver.endSession)
	{
		gui_state = GuiState::EndSpectate;
	}
*/

	while (this_frame.empty() && !disconnect_toggle)
	{
		try {
			this_frame = net_inputs[port].at(FrameNumber - 1);

			if (!this_frame.empty())
			{
				frame_timeout = 0;

				if (config::Debug == DEBUG_APPLY ||
					config::Debug == DEBUG_APPLY_BACKFILL ||
					config::Debug == DEBUG_APPLY_BACKFILL_RECV ||
					config::Debug == DEBUG_ALL)
				{
					PrintFrameData("Applied", (u8*)this_frame.data());
				}
			}
		}
		catch (const std::out_of_range& oor)
		{
			// fill audio stream with 0 to mute repeated frame audio
			for (int i = 0; i < 512; i++)
				WriteSample(0, 0);

			frame_timeout++;

			if (!hosting && frame_timeout > delay)
				client.request_repeat = true;

			// give ~40 seconds for connection to continue
			if (!config::Receiving && frame_timeout > 2400)
				disconnect_toggle = true;

			// give ~30 seconds for spectating connection to continue
			if (config::Receiving && frame_timeout > 3600)
				receiver_ended = true;

			if (config::Receiving &&
				FrameNumber >= last_consecutive_common_frame)
			{
				pause();
			}

		};
	}

	std::string to_apply(this_frame);

	if (config::RecordMatches && !dojo.PlayMatch)
	{
		if (GetEffectiveFrameNumber((u8*)this_frame.data()) >= SkipFrame ||
			settings.platform.system != DC_PLATFORM_NAOMI)
		{
			AppendToReplayFile(this_frame);
		}
	}

	if (transmitter_started)
		dojo.transmission_frames.push_back(this_frame);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		TranslateFrameDataToInput((u8*)to_apply.data(), pjs);
	else
		buttons = TranslateFrameDataToInput((u8*)to_apply.data(), buttons);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		return 0;
	else
		return buttons;
}

// called on by client thread once data is received
void DojoSession::ClientReceiveAction(const char* received_data)
{
	if (config::Debug == DEBUG_RECV ||
		config::Debug == DEBUG_SEND_RECV ||
		config::Debug == DEBUG_ALL)
	{
		PrintFrameData("Received", (u8*)received_data);
	}

	if (client_input_authority && GetPlayer((u8*)received_data) == player)
		return;

	std::string to_add(received_data, received_data + FRAME_SIZE);
	AddNetFrame(to_add.data());
	if (config::EnableBackfill)
		AddBackFrames(to_add.data(), received_data + FRAME_SIZE, dojo.PayloadSize() - FRAME_SIZE);
}

// continuously called on by client thread
void DojoSession::ClientLoopAction()
{
	if (last_consecutive_common_frame < dojo.FrameNumber)
		pause();

	if (last_consecutive_common_frame == dojo.FrameNumber)
		resume();
}

std::string currentISO8601TimeUTC() {
	auto now = std::chrono::system_clock::now();
	auto itt = std::chrono::system_clock::to_time_t(now);
#ifndef _MSC_VER
	char buf[128] = { 0 };
	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&itt));
	return buf;
#else
	std::ostringstream ss;
	ss << std::put_time(gmtime(&itt), "%FT%TZ");
	return ss.str();
#endif
}

std::string DojoSession::CreateReplayFile()
{
	std::string rom_name = GetRomNamePrefix();
	return CreateReplayFile(rom_name);
}

std::string DojoSession::CreateReplayFile(std::string rom_name)
{
	// create timestamp string, iso8601 format
	std::string timestamp = currentISO8601TimeUTC();
	std::replace(timestamp.begin(), timestamp.end(), ':', '_');
	std::string filename =
		"replays/" + rom_name +  "__" +
		timestamp + "__" +
		config::PlayerName.get() + "__" + 
		config::OpponentName.get() + "__" + 
		".flyreplay";
	// create replay file itself
	std::ofstream file;
	file.open(filename);
	// TODO define metadata at beginning of file

	dojo.ReplayFilename = filename;
	dojo.replay_filename = filename;

	return filename;
}

void DojoSession::AppendToReplayFile(std::string frame)
{
	if (frame.size() == FRAME_SIZE)
	{
		// append frame data to replay file
		std::ofstream fout(replay_filename, 
			std::ios::out | std::ios::binary | std::ios_base::app);

		fout.write(frame.data(), FRAME_SIZE);
		fout.close();
	}
}

void DojoSession::LoadReplayFile(std::string path)
{
	std::string ppath = path;
	stringfix::replace(ppath, "__", ":");
	std::vector<std::string> name_info = stringfix::split(":", ppath);
	config::PlayerName = name_info[3];
	config::OpponentName = name_info[4];

	// add string in increments of FRAME_SIZE to net_inputs
	std::ifstream fin(path, 
		std::ios::in | std::ios::binary);

	char* buffer = new char[FRAME_SIZE];

	while (fin)
	{
		fin.read(buffer, FRAME_SIZE);
		size_t count = fin.gcount();

		int player_num = GetPlayer((u8*)buffer);
		u32 frame_num = GetEffectiveFrameNumber((u8*)buffer);

		if (count == FRAME_SIZE &&
			frame_num == SkipFrame)
		{
			FillSkippedFrames(SkipFrame);
			last_consecutive_common_frame = SkipFrame - 1;
		}

		if (net_inputs[player_num].count(frame_num) == 0)
		{
			net_inputs[player_num][frame_num] = std::string(buffer, FRAME_SIZE);
			net_input_keys[player_num].insert(frame_num);
		}	

		if (!count)
			break;
	}

	delete[] buffer;
}

void DojoSession::RequestSpectate(std::string host, std::string port)
{
	// start receiver before game load so host can immediately respond
	std::thread t5(&DojoSession::receiver_thread, std::ref(dojo));
	t5.detach();

	receiver_started = true;

	using asio::ip::udp;

	asio::io_context io_context;
	udp::socket s(io_context, udp::endpoint(udp::v4(), 0));

	udp::resolver resolver(io_context);
	udp::resolver::results_type endpoints =
		resolver.resolve(udp::v4(), host, port);

	std::string request = "SPECTATE " + config::SpectatorPort.get();
	for (int i = 0; i < packets_per_frame; i++)
	{
		s.send_to(asio::buffer(request, request.length()), *endpoints.begin());
	}

	s.close();
}

void DojoSession::receiver_thread()
{
	try
	{
		asio::io_context io_context;
		AsyncTcpServer s(io_context, atoi(config::SpectatorPort.get().data()));
		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}

void DojoSession::transmitter_thread()
{
	try
	{
		asio::io_context io_context;

		tcp::resolver resolver(io_context);
		tcp::resolver::results_type endpoints =
			resolver.resolve(config::SpectatorIP, config::SpectatorPort);

		tcp::socket socket(io_context);
		asio::connect(socket, endpoints);

		transmitter_started = true;
		std::string current_frame;
		volatile bool transmission_in_progress;

		std::string header_data = get_game_name() + "," +
			config::PlayerName.get() + "," +
			config::OpponentName.get() + ",";

		if (config::EnableMatchCode)
			header_data = header_data.append(config::MatchCode.get() + ",");
		else
			header_data = header_data.append(",");

		header_data.append(256 - header_data.length(), 0);

		asio::write(socket, asio::buffer(header_data.data(), HEADER_SIZE));
		std::cout << "Transmission Started" << std::endl;

		for (;;)
		{
			transmission_in_progress = !dojo.transmission_frames.empty() && !transmitter_ended;

			if (transmission_in_progress)
			{
				current_frame = transmission_frames.front();

				asio::write(socket, asio::buffer(current_frame.data(), FRAME_SIZE));
				//std::cout << PrintFrameData("SENT", (u8*)current_frame.data()) << std::endl;

				transmission_frames.pop_front();
			}

			if (transmitter_ended ||
				(disconnect_toggle && !transmission_in_progress))
			{
				asio::write(socket, asio::buffer("000000000000", FRAME_SIZE));
				std::cout << "Transmission Ended" << std::endl;
				break;
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}

u16 DojoSession::ApplyOfflineInputs(PlainJoystickState* pjs, u16 buttons, u32 port)
{
	u32 target_port = port;

	if (player == 1)
	{
		target_port = (port == 0 ? 1 : 0);
	}

	std::string current_frame_data;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		current_frame_data = std::string((const char*)TranslateInputToFrameData(pjs, 0, target_port), FRAME_SIZE);
	else
		current_frame_data = std::string((const char*)TranslateInputToFrameData(0, buttons, target_port), FRAME_SIZE);

	AddNetFrame(current_frame_data.data());

	if (config::Training)
	{
		if (recording && GetPlayer((u8*)current_frame_data.data()) == record_player)
			record_slot.push_back(current_frame_data);

		if (!recording && !playing_input &&
			playback_loop && trigger_playback &&
			FrameNumber > next_playback_frame)
		{
			PlayRecording();
		}
	}

	if (delay > 0 || config::Training)
	{
		std::string this_frame;
		while(this_frame.empty())
			this_frame = net_inputs[port].at(FrameNumber);

		if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
			settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		{
			PlainJoystickState blank_pjs;
			memcpy(pjs, &blank_pjs, sizeof(blank_pjs));
			TranslateFrameDataToInput((u8*)this_frame.data(), pjs);
		}
		else
		{
			buttons = TranslateFrameDataToInput((u8*)this_frame.data(), buttons);
		}
	}

	if (net_inputs[0].count(FrameNumber + delay) == 1 &&
		net_inputs[1].count(FrameNumber + delay) == 1)
	{
		if (config::RecordMatches)
		{
			AppendToReplayFile(net_inputs[0].at(FrameNumber + delay));
			AppendToReplayFile(net_inputs[1].at(FrameNumber + delay));
		}

		FrameNumber++;
	}

	if (settings.platform.system == DC_PLATFORM_DREAMCAST ||
		settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		return 0;
	else
		return buttons;
}

void DojoSession::ToggleRecording()
{
	std::ostringstream NoticeStream;
	if (recording)
	{
		recording = false;
		NoticeStream << "Player " << record_player + 1 << " Recording Stopped";
	}
	else
	{
		record_slot.clear();
		recording = true;
		NoticeStream << "Player " << record_player + 1 << " Recording Started";
	}
	gui_display_notification(NoticeStream.str().data(), 2000);

}

void DojoSession::TogglePlayback()
{
	if (dojo.playback_loop)
	{
		if (dojo.trigger_playback)
		{
			dojo.trigger_playback = false;
			gui_display_notification("Stopping Playback Loop", 2000);
		}
		else
		{
			dojo.trigger_playback = true;
			gui_display_notification("Starting Playback Loop", 2000);
		}
	}
	else
	{
		dojo.PlayRecording();
	}
}

void DojoSession::PlayRecording()
{
	if (!recording && !playing_input)
	{
		playing_input = true;
		u8 to_add[FRAME_SIZE] = { 0 };
		u32 target_frame = dojo.FrameNumber + 1 + dojo.delay;
		for (std::string frame : dojo.record_slot)
		{
			//to_add[0] = (u8)port;
			memcpy(to_add, frame.data(), FRAME_SIZE);
			memcpy(to_add + 2, (u8*)&target_frame, 4);
			AddNetFrame((const char *)to_add);
			target_frame++;
		}
		next_playback_frame = target_frame;
		playing_input = false;
	}
}

void DojoSession::SwitchPlayer()
{
	record_player == 0 ?
		record_player = 1 :
		record_player = 0;

	player == 0 ?
		player = 1 :
		player = 0;

	opponent = player == 0 ? 1 : 0;

	std::ostringstream NoticeStream;
	NoticeStream << "Monitoring Player " << record_player + 1;
	gui_display_notification(NoticeStream.str().data(), 2000);
}

DojoSession dojo;

