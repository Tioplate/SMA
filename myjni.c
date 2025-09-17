#include "myjni.h"

JNIEXPORT jobject JNICALL Java_com_research_sma_controller_SMAController_runSMA(JNIEnv *env, jobject obj, jstring jDataPath, jint pop, jint dim)
{
    const char *dataPath = (*env)->GetStringUTFChars(env, jDataPath, 0); // "../dataset/rc_205.3.txt"
    FIT_DATA_TYPE lb[dim];
    FIT_DATA_TYPE ub[dim];
    for (int i = 0; i < dim; i++)
    {
        lb[i] = 0;
        ub[i] = dim;
    }
    SMAResult *result;
    result = SMA(pop, dim, lb, ub, dataPath, 100);
    jclass resultClass = (*env)->FindClass(env, "com/research/sma/entity/SMAResult");
    jmethodID ctor = (*env)->GetMethodID(env, resultClass, "<init>", "(IIID[D[D)V");
    jdoubleArray jCurve = (*env)->NewDoubleArray(env, result->iterationATime);
    (*env)->SetDoubleArrayRegion(env, jCurve, 0, result->iterationATime, result->convergenceCurve);
    jdoubleArray jBest = (*env)->NewDoubleArray(env, result->dimension);
    (*env)->SetDoubleArrayRegion(env, jBest, 0, result->dimension, result->bestPositions);

    jobject jResult = (*env)->NewObject(env, resultClass, ctor,
                                        result->pop,
                                        result->dimension,
                                        result->iterationATime,
                                        result->destinationFitness,
                                        jCurve,
                                        jBest);
    free(result->bestPositions);
    free(result->convergenceCurve);
    free(result);
    (*env)->ReleaseStringUTFChars(env, jDataPath, dataPath);

    return jResult;
}