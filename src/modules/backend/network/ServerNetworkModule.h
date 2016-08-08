/**
 * @file
 */

#pragma once

#include "network/NetworkModule.h"
#include "ClientMessages_generated.h"

#include "UserConnectHandler.h"
#include "UserConnectedHandler.h"
#include "UserDisconnectHandler.h"
#include "AttackHandler.h"
#include "MoveHandler.h"

namespace backend {

class ServerNetworkModule: public NetworkModule {
	template<typename Ctor>
	inline void bindHandler(network::ClientMsgType type) const {
		bind<network::IProtocolHandler>().named(network::EnumNameClientMsgType(type)).to<Ctor>();
	}

	void configureHandlers() const override {
		bindHandler<UserConnectHandler(network::Network &, backend::EntityStorage &, voxel::World &)>(network::ClientMsgType::UserConnect);
		bindHandler<UserConnectedHandler>(network::ClientMsgType::UserConnected);
		bindHandler<UserDisconnectHandler>(network::ClientMsgType::UserDisconnect);
		bindHandler<AttackHandler>(network::ClientMsgType::Attack);
		bindHandler<MoveHandler>(network::ClientMsgType::Move);
	}
};

}
