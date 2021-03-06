LOCAL_PATH:= $(call my-dir)

# libogg
include $(CLEAR_VARS)

LOCAL_MODULE    := libdevil
LOCAL_CFLAGS    := -fexceptions -g -Dlinux -Dunix \
	-DHAVE_GCC_DESTRUCTOR=1 -DOPT_GENERIC -DREAL_IS_FLOAT 

LOCAL_CPPFLAGS  := ${LOCAL_CFLAGS}

LOCAL_C_INCLUDES  :=  \
	${LOCAL_PATH}/include \
	${LOCAL_PATH}/src-IL/include \
	${LOCAL_PATH}/../jasper-1.900.1/src/libjasper/include \
	${LOCAL_PATH}/../libpng-1.6.8/ \
	${LOCAL_PATH}/../libmng-1.0.10/ \
	${LOCAL_PATH}/../jpeg-9a/ \
	${LOCAL_PATH}/../lcms2-2.5/include \
	${LOCAL_PATH}/../tiff-3.9.5/libtiff 
		
LOCAL_SRC_FILES := \
	$(filter-out \
	,$(subst $(LOCAL_PATH)/,,\
	$(wildcard ${LOCAL_PATH}/src-IL/src/*.cpp) \
  $(wildcard ${LOCAL_PATH}/src-IL/src/*.c) ))

LOCAL_SHARED_LIBRARIES := libtiff libpng libjasper libjpeg libmng liblcms

# $(info libdevil: include dirs $(LOCAL_C_INCLUDES))
# $(info libdevil: src files $(LOCAL_SRC_FILES))

include $(BUILD_SHARED_LIBRARY)
