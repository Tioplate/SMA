#pragma once
#include "jni.h"
#include <stdlib.h>
#include <string.h>
#include "sma.h"
JNIEXPORT jobject JNICALL Java_com_research_sma_controller_SMAController_runSMA(JNIEnv *env, jobject obj, jstring jDataPath, jint pop, jint dim);