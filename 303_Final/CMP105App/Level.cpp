#include "Level.h"

Level::Level(sf::RenderWindow* hwnd, Input* in)
{
	window = hwnd;
	input = in;



	//view1.reset(sf::FloatRect(0, 0, window->getSize().x, window->getSize().y));

	mainMenu.setup(hwnd, in);
	lobby.setup(hwnd, in);
	game.setup(hwnd, in);

	networkModule.setup();


}

Level::~Level()
{

}



// handle user input
void Level::handleInput(float dt)
{
	if (input->isKeyDown(sf::Keyboard::Escape)) {
		window->close();
	}

	if (input->isKeyDown(sf::Keyboard::Comma)) {
		lobby.addPeer(std::to_string(rand() % 1500),"test", "test");
	}

}

// Update game objects
void Level::update(float dt)
{
	switch (gameState) {
	case MAIN_MENU: //MAIN MENU UPDATE
		mainMenu.update(dt);
		if (mainMenu.attemptConnect) {//if player clicked enter on the connect screen
			mainMenu.attemptConnect = false;
			networkModule.setMyName(mainMenu.getEnteredName());
			game.setMyName(mainMenu.getEnteredName());//commented under is just ip address enter disabled
			
			//if user entered 1 as ip, connect to this local address
			sf::IpAddress connectAddress = sf::IpAddress(mainMenu.getEnteredIP());
			if (mainMenu.getEnteredIP() == "1") {//connect locally
				connectAddress = sf::IpAddress::getLocalAddress();
			}

			connectResult connectResult_ = networkModule.connect_TCP_to(connectAddress, (unsigned short)std::stoi(mainMenu.getEnteredPort()), true);
			
			if (connectResult_ == SUCCESS) {
				mainMenu.goToLobby = true;
			}
			else {
				mainMenu.resetInput(true, connectResult_);
			}
			
		}
		if (mainMenu.goToLobby) {
			input->setMouseLDown(false);
			game.setMyName(mainMenu.getEnteredName());
			networkModule.setMyName(mainMenu.getEnteredName());
			lobby.addPeer(networkModule.getMyInfo()->name, networkModule.getMyInfo()->IpAddress.toString(), std::to_string(networkModule.getMyInfo()->TCP_listener_Port));
			gameState = LOBBY;
		}
		break;

	case LOBBY: //LOBBY CODE
		networkModule.accept_TCP_new();
		//check if there's anyone new to add
		if (lobby.displayedPeerNr() < networkModule.getPeerCount()) {
			for (int i = 0; i < networkModule.getPeerCount(); i++) {
				lobby.addPeer(networkModule.getPeer(i)->name, networkModule.getPeer(i)->IpAddress.toString(), std::to_string(networkModule.getPeer(i)->TCP_listener_Port)); //display peers
			}
		}
		if (networkModule.someoneDisconnected) {
			networkModule.someoneDisconnected = false;
			lobby.disconnectPlayer(networkModule.disconnectedName);
		}

		if (lobby.chat.sentSomething) {
			sf::Packet packet;
			header hdr_;
			hdr_.game_elapsed_time = 0.0f;
			hdr_.information_amount = 1;
			hdr_.information_type = CHAT_MESSAGE;
			hdr_.senderName = networkModule.getMyInfo()->name;
			packet << hdr_ << lobby.chat.sent_string;
			networkModule.pushOutPacket_all(packet);
			lobby.chat.sentSomething = false;
			lobby.chat.sent_string = "";
			networkModule.sendAll_TCP();
		}
		lobby.update(dt);
		if (lobby.startGame) {
			for (int i = 0; i < networkModule.getPeerCount(); i++) {
				game.addEnemy(networkModule.getPeer(i)->name);
			}
			gameState = GAME;
		}
		break;

	case GAME:
		networkModule.accept_TCP_new();

		if (networkModule.someoneDisconnected) {
			networkModule.someoneDisconnected = false;
			game.disconnectPlayer(networkModule.disconnectedName);

		}

		game.update(dt);

		
		break;

	}

	nwShareTimer -= dt;

	if (nwShareTimer <= 0.0f) {
		bool imReady = false;
		nwShareTimer = 0.1f;
		sf::Packet packet;
		header hdr_;
		hdr_.game_elapsed_time = game.getClock()->getElapsedTime().asSeconds();
		hdr_.senderName = networkModule.getMyInfo()->name;
		switch (gameState) {
		case LOBBY:
			hdr_.game_elapsed_time = lobby.countDownTimer;
			hdr_.information_amount = 1;
			hdr_.information_type = LOBBY_READY_STATUS;
			if (lobby.readyButton.fillColour == sf::Color::Green) {
				imReady = true;
			}
			packet << hdr_ << imReady;
			networkModule.pushOutPacket_all(packet);
			networkModule.sendAll_TCP();
			break;

		case GAME:
			hdr_.information_amount = 1;
			hdr_.information_type = PLAYER_POS_ANGLE;
			playerPosLookDir myPLD = game.getplayerPosLookDir();
			packet << hdr_ << myPLD.position.x << myPLD.position.y << myPLD.lookDir.x << myPLD.lookDir.y;
			networkModule.pushOutPacket_all(packet);
			networkModule.sendAll_TCP();
			break;

		}
		
	}

	decodeImportantGameEvs();
	networkModule.receiveAll_TCP();
	while (networkModule.anyPacketsToRead()) {
		decodePacket(networkModule.getPacketToRead());
	}
}

// Render level
void Level::render()
{
	beginDraw();

	switch (gameState) {
	case MAIN_MENU:
		mainMenu.render();
		break;

	case LOBBY:
		lobby.render();
		break;

	case GAME:
		game.render();
		break;
	}


	endDraw();
}


// Begins rendering to the back buffer. Background colour set to light blue.
void Level::beginDraw()
{
	window->clear(sf::Color(100, 100, 100));
}

// Ends rendering to the back buffer, and swaps buffer to the screen.
void Level::endDraw()
{
	window->display();
}

void Level::decodeImportantGameEvs() {
	while (game.areImportantEv()) {
		eventInfo ev = game.getImportantEv();
		sf::Packet packet;
		header header_;
		header_.game_elapsed_time = 0.0f;
		header_.information_amount = 1;
		header_.senderName = networkModule.getMyInfo()->name;
		//header_.information_type = 
		switch (ev.type) {
			case AREA_CAPTURED_:
				header_.information_type = AREA_CAPTURED;
				packet << header_ << (sf::Uint16)ev.v1.x << (sf::Uint16)ev.v1.y;
				networkModule.pushOutPacket_all(packet);
			break;

			case BULLET_SHOT_:
				header_.information_type = BULLET_SHOT;
				packet << header_; 
				packet << (sf::Uint16)ev.a;//id
				packet << ev.v1.x << ev.v1.y; //spawn pos
				packet << ev.v2.x << ev.v2.y; //direction
				networkModule.pushOutPacket_all(packet);
			break;

			case PLAYER_HIT_:
				header_.information_type = PLAYER_HIT;
				packet << header_;
				packet << (sf::Uint16)ev.a;//bullet id
				packet << ev.str; //hit player name
				networkModule.pushOutPacket_all(packet);
			break;
		}
	}

}

void Level::decodePacket(sf::Packet packet) {

	sf::Uint32 int32holder;
	char* charbbuffer = new char;
	peerNWinfo info_;
	playerPosLookDir receivedPPLD;
	sf::Vector2f vector1, vector2;
	sf::Uint16 a, b;
	bool boolVar;
	//deal with header here

	header header_;
	packet >> header_;
	for (int i = 0; i < header_.information_amount; i++) {
		switch (header_.information_type) {

		case NW_INFO:
			packet >> info_;
			if (networkModule.connect_TCP_to(sf::IpAddress(info_.ipAddress), (unsigned short)info_.listenerPort, 0)) {
				lobby.addPeer(networkModule.getLastAddedPeer()->name, sf::IpAddress(info_.ipAddress).toString(), std::to_string((unsigned short)info_.listenerPort));
			}
		break;

		case CHAT_MESSAGE:
			packet >> charbbuffer;
			if (gameState == LOBBY) {
				lobby.chat.addMessage(charbbuffer, sf::Color::White, header_.senderName);
			}
		break;

		case PLAYER_POS_ANGLE:
			packet >> receivedPPLD.position.x >> receivedPPLD.position.y >> receivedPPLD.lookDir.x >> receivedPPLD.lookDir.y;
			game.updateEnemyVals(header_.senderName, receivedPPLD);
		break;

		case AREA_CAPTURED:
			packet >> a >> b;
			game.captureTile(a, b, header_.senderName);
		break;

		case BULLET_SHOT:
			packet >> a;
			packet >> vector1.x >> vector1.y; //spawn pos
			packet >> vector2.x >> vector2.y; //direction

			game.addEnemyBullet(Bullet(vector1, vector2, a));
		break;

		case PLAYER_HIT:
			packet >> a;//bullet id
			packet >> charbbuffer; //hit player name
			game.enemyGotHit(charbbuffer, a);
		break;

		default:
		break;

		case LOBBY_READY_STATUS:
			packet >> boolVar; //hit player name
			lobby.setReady(header_.senderName, boolVar);
			if (header_.game_elapsed_time < lobby.countDownTimer) {
				lobby.countDownTimer = header_.game_elapsed_time;
			}
		break;
		}
	}
}