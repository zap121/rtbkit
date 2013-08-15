#------------------------------------------------------------------------------#
# filters.mk
# Rémi Attab, 25 Jul 2013
# Copyright (c) 2013 Datacratic.  All rights reserved.
#
# Router filter makefile
#------------------------------------------------------------------------------#

LIB_FILTERS_SOURCES := \
	static_filters.cc \
        creative_filters.cc

LIB_FILTERS_LINK := \
	arch utils filter_registry agent_configuration rtb

$(eval $(call library,filters,$(LIB_FILTERS_SOURCES),$(LIB_FILTERS_LINK)))

$(eval $(call include_sub_make,filters_test,testing,filters_test.mk))
