# RTBKIT exchange makefile

LIBRTB_EXCHANGE_SOURCES := \
	exchange_connector.cc \
	http_exchange_connector.cc \
	http_auction_handler.cc \
	ad_server_connector.cc

LIBRTB_EXCHANGE_LINK := \
	zeromq boost_thread utils endpoint services rtb post_auction

$(eval $(call library,exchange,$(LIBRTB_EXCHANGE_SOURCES),$(LIBRTB_EXCHANGE_LINK)))

$(eval $(call library,rubicon_exchange,rubicon_exchange_connector.cc,exchange openrtb_bid_request))

$(eval $(call include_sub_make,exchange_testing,testing,exchange_testing.mk))
