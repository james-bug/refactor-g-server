# 
# Copyright (C) 2025 Gaming System Development Team
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gaming-server
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk


define Package/gaming-server
  SECTION:=BenQ
  CATEGORY:=BenQ
  TITLE:=Gaming Server Daemon
  SUBMENU:=Applications
  DEPENDS:=+gaming-core +gaming-platform +libwebsockets-full +libuci +cJSON
endef



define Package/gaming-server/description
  Gaming Client Daemon for Travel Router.
  Provides button control, VPN connection management,
  and PS5 status query via WebSocket.
endef

define Package/gaming-server/conffiles
/etc/config/gaming-server
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/gaming \
		-I../gaming-core/src \
		-I../gaming-core/src/hal \
		-o $(PKG_BUILD_DIR)/gaming-server \
		$(PKG_BUILD_DIR)/main.c \
                $(PKG_BUILD_DIR)/cec_monitor.c \
                $(PKG_BUILD_DIR)/ps5_detector.c \
                $(PKG_BUILD_DIR)/ps5_wake.c \
                $(PKG_BUILD_DIR)/websocket_server.c \
                $(PKG_BUILD_DIR)/server_state_machine.c \
		-L$(STAGING_DIR)/usr/lib \
		-L$(STAGING_DIR)/root-mediatek/usr/lib \
		-lgaming-core \
		-lgaming-platform \
		-lwebsockets \
		-lcjson \
		-luci \
		-lpthread \
		-lrt \
		-lm
endef

define Package/gaming-server/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gaming-server $(1)/usr/bin/
	
endef


$(eval $(call BuildPackage,gaming-server))
