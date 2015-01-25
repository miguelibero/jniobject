
#include "JniObject.hpp"
#include <algorithm>
#include <pthread.h>

JavaVM* Jni::_java = nullptr;
JNIEnv* Jni::_env = nullptr;
int Jni::_thread = 0;
 
Jni::Jni()
{
}
 
Jni::Jni(const Jni& other)
{
    assert(false);
}
 
Jni::~Jni()
{
    if(!_classes.empty())
    {
        JNIEnv* env = getEnvironment();
        if(env)
        {
            for(ClassMap::const_iterator itr = _classes.begin(); itr != _classes.end(); ++itr)
            {
                env->DeleteGlobalRef(itr->second);
            }
        }
    }
}
 
Jni& Jni::get()
{
    static Jni jni;
    return jni;
}
 
JavaVM* Jni::getJava()
{
    return _java;
}

void Jni::detachCurrentThread(void*)
{
  _java->DetachCurrentThread();
}
 
jint Jni::onLoad(JavaVM* java)
{
    JNIEnv *env = NULL;
    if (java->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK)
    {
        return 0;
    } 
    _java = java;
    pthread_key_create(&_thread, Jni::detachCurrentThread);
    return JNI_VERSION_1_4;
}
 
JNIEnv* Jni::getEnvironment()
{
    if(_java == nullptr)
    {
        throw JniException("Jni::onLoad not called.");
    }
    JNIEnv* env = static_cast<JNIEnv*>(pthread_getspecific(_thread));
    if((env ) == NULL)
    {
        _java->GetEnv((void**)&_env, JNI_VERSION_1_4);
        _java->AttachCurrentThread(&env, nullptr);
        pthread_setspecific(_thread, env);
    }
    return env;
}
 
jclass Jni::getClass(const std::string& classPath, bool cache)
{
    ClassMap::const_iterator itr = _classes.find(classPath);
    if(itr != _classes.end())
    {
        return itr->second;
    }
    JNIEnv* env = getEnvironment();
    if(env)
    {
        jclass cls = (jclass)env->FindClass(classPath.c_str());
        if (cls)
        {
            if(cache)
            {
                cls = (jclass)env->NewGlobalRef(cls);
                _classes[classPath] = cls;
                return cls;
            }
            else
            {
                return cls;
            }
        }
        else
        {
            env->ExceptionClear();
        }
    }
    return nullptr;
}

#pragma mark - JniObject
 
JniObject::JniObject(const std::string& classPath, jobject objId, jclass classId) :
_instance(nullptr), _class(nullptr)
{
    init(objId, classId, classPath);
}
 
JniObject::JniObject(jclass classId, jobject objId) :
_instance(nullptr), _class(nullptr)
{
    init(objId, classId);
}
 
JniObject::JniObject(jobject objId) :
_instance(nullptr), _class(nullptr)
{
    init(objId);
}
 
JniObject::JniObject(const JniObject& other) :
_instance(nullptr), _class(nullptr)
{
    init(other._instance, other._class, other._classPath);
}
 
void JniObject::init(jobject objId, jclass classId, const std::string& classPath)
{
    JNIEnv* env = getEnvironment();
    _classPath = classPath;
    std::replace(_classPath.begin(), _classPath.end(), '.', '/');
    if(env)
    {
        if(!classId)
        {
            if(objId)
            {
                classId = env->GetObjectClass(objId);
            }
            else if(!classPath.empty())
            {
                classId = Jni::get().getClass(_classPath);
            }
        }
        if(classId)
        {
            _class = (jclass)env->NewGlobalRef(classId);
        }
        else
        {
            _classPath = "";
        }
        if(objId)
        {
            _instance = env->NewGlobalRef(objId);
        }
    }
}
 
JniObject::~JniObject()
{
    clear();
}

void JniObject::checkJniException()
{
    JNIEnv* env = getEnvironment();
    if(!env)
    {
        return;
    }
    if (!env->ExceptionCheck())
    {
        return;
    }
    JniObject exc(env->ExceptionOccurred());
    if(exc)
    {
        env->ExceptionClear();
        std::string msg = exc.getClassPath()+": ";
        msg += exc.call("getLocalizedMessage", msg);
        throw JniException(msg);
    }

}
 
void JniObject::clear()
{
    JNIEnv* env = getEnvironment();
    if(!env)
    {
        return;
    }
    if(_class)
    {
        env->DeleteGlobalRef(_class);
        _class = nullptr;
    }
    if(_instance)
    {
        env->DeleteGlobalRef(_instance);
        _instance = nullptr;
    }
}
 
std::string JniObject::getSignature() const
{
    return std::string("L")+getClassPath()+";";
}
 
const std::string& JniObject::getClassPath() const
{
    if(_classPath.empty() && _class)
    {
        try
        {
            _classPath = JniObject("java/lang/Class", _class).call("getName", _classPath);
        }
        catch(JniException e)
        {
        }
    }
    return _classPath;
}
 
JNIEnv* JniObject::getEnvironment()
{
    return Jni::get().getEnvironment();
}
 
jclass JniObject::getClass() const
{
    return _class;
}
 
jobject JniObject::getInstance() const
{
    return _instance;
}
 
jobject JniObject::getNewLocalInstance() const
{
    JNIEnv* env = getEnvironment();
    if(!env)
    {
        return 0;
    }
    return env->NewLocalRef(getInstance());
}
 
bool JniObject::isInstanceOf(const std::string& classPath) const
{
    std::string fclassPath(classPath);
    std::replace(fclassPath.begin(), fclassPath.end(), '.', '/');
    JNIEnv* env = getEnvironment();
    if(!env)
    {
        return false;
    }
    jclass cls = env->FindClass(fclassPath.c_str());
    return env->IsInstanceOf(getInstance(), cls);
}
 
JniObject JniObject::findSingleton(const std::string& classPath)
{
    JniObject cls(classPath);
    try
    {
        return cls.staticField("instance", cls);
    }
    catch(JniException e)
    {
    }
    try
    {
        return cls.staticCall("getInstance", cls);
    }
    catch(JniException e)
    {
    }    
    throw JniException("Could not find singleton instance.");
}
 
JniObject::operator bool() const
{
    return getInstance() != nullptr;
}
 
JniObject& JniObject::operator=(const JniObject& other)
{
    clear();
    _classPath = other._classPath;
    init(other._instance, other._class);
    return *this;
}
 
bool JniObject::operator==(const JniObject& other) const
{
    JNIEnv* env = getEnvironment();
    if(!env)
    {
        return false;
    }
    jobject a = getInstance();
    jobject b = other.getInstance();
    if(a && b)
    {
        return env->IsSameObject(a, b);
    }
    a = getClass();
    b = other.getClass();
    return env->IsSameObject(a, b);
}
 
#pragma mark - JniObject::getSignaturePart
 
template<>
std::string JniObject::getSignaturePart<std::string>(const std::string& val)
{
    return "Ljava/lang/String;";
}
 
template<>
std::string JniObject::getSignaturePart(const JniObject& val)
{
    return val.getSignature();
}
 
template<>
std::string JniObject::getSignaturePart(const bool& val)
{
    return "Z";
}

template<>
std::string JniObject::getSignaturePart(const uint8_t& val)
{
    return "B";
}
 
template<>
std::string JniObject::getSignaturePart(const char& val)
{
    return "C";
}
 
template<>
std::string JniObject::getSignaturePart(const short& val)
{
    return "S";
}
 
template<>
std::string JniObject::getSignaturePart(const long& val)
{
    return "J";
}
 
template<>
std::string JniObject::getSignaturePart(const int& val)
{
    return "I";
}
 
template<>
std::string JniObject::getSignaturePart(const unsigned int& val)
{
    return "I";
}
 
template<>
std::string JniObject::getSignaturePart(const float& val)
{
    return "F";
}
 
template<>
std::string JniObject::getSignaturePart(const jobject& val)
{
    return getSignaturePart(JniObject(val));
}
 
std::string JniObject::getSignaturePart()
{
    return "V";
}
 
#pragma mark - JniObject::convertToJavaValue
 
template<>
jvalue JniObject::convertToJavaValue(const bool& obj)
{
    jvalue val;
    val.z = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const uint8_t& obj)
{
    jvalue val;
    val.b = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const char& obj)
{
    jvalue val;
    val.c = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const short& obj)
{
    jvalue val;
    val.s = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const int& obj)
{
    jvalue val;
    val.i = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const unsigned int& obj)
{
    jvalue val;
    val.i = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const long& obj)
{
    jvalue val;
    val.j = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const long long& obj)
{
    jvalue val;
    val.j = obj;
    return val;
}

template<>
jvalue JniObject::convertToJavaValue(const float& obj)
{
    jvalue val;
    val.f = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const double& obj)
{
    jvalue val;
    val.d = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const jobject& obj)
{
    jvalue val;
    val.l = obj;
    return val;
}
 
template<>
jvalue JniObject::convertToJavaValue(const JniObject& obj)
{
    return convertToJavaValue(obj.getInstance());
}
 
template<>
jvalue JniObject::convertToJavaValue(const std::string& obj)
{
    JNIEnv* env = getEnvironment();
    if (!env)
    {
        return jvalue();
    }
    return convertToJavaValue(env->NewStringUTF(obj.c_str()));
}
 
#pragma mark - JniObject::convertFromJavaObject
 
template<>
bool JniObject::convertFromJavaObject(JNIEnv* env, jobject obj, std::string& out)
{
    if(!obj)
    {
        out = "";
        return true;
    }
    jstring jstr = (jstring)obj;
    const char* chars = env->GetStringUTFChars(jstr, NULL);
    if(!chars)
    {
        return false;
    }
    out = chars;
    env->ReleaseStringUTFChars(jstr, chars);
    return true;
}
 
template<>
bool JniObject::convertFromJavaObject(JNIEnv* env, jobject obj, JniObject& out)
{
    out = obj;
    env->DeleteLocalRef(obj);
    return true;
}
 
#pragma mark - JniObject call jni
 
template<>
void JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return env->CallStaticVoidMethodA(classId, methodId, args);
}
 
template<>
jobject JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return env->CallStaticObjectMethodA(classId, methodId, args);
}
 
template<>
double JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return env->CallStaticDoubleMethodA(classId, methodId, args);
}
 
template<>
long JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return env->CallStaticLongMethodA(classId, methodId, args);
}
 
template<>
float JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return env->CallStaticFloatMethodA(classId, methodId, args);
}
 
template<>
int JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return env->CallStaticIntMethodA(classId, methodId, args);
}
 
template<>
std::string JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return convertFromJavaObject<std::string>(env, callStaticJavaMethod<jobject>(env, classId, methodId, args));
}
 
template<>
JniObject JniObject::callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args)
{
    return convertFromJavaObject<JniObject>(env, callStaticJavaMethod<jobject>(env, classId, methodId, args));
}
 
void JniObject::callJavaVoidMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args)
{
    env->CallVoidMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, bool& out)
{
    out = env->CallBooleanMethodA(objId, methodId, args);
}

template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, uint8_t& out)
{
    out = env->CallByteMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, jobject& out)
{
    out = env->CallObjectMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, double& out)
{
    out = env->CallDoubleMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, long& out)
{
    out = env->CallLongMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, float& out)
{
    out = env->CallFloatMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, int& out)
{
    out = env->CallIntMethodA(objId, methodId, args);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, std::string& out)
{
    callJavaObjectMethod(env, objId, methodId, args, out);
}
 
template<>
void JniObject::callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, JniObject& out)
{
    callJavaObjectMethod(env, objId, methodId, args, out);
}
 
template<>
jobject JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return env->GetStaticObjectField(classId, fieldId);
}
 
template<>
double JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return env->GetStaticDoubleField(classId, fieldId);
}
 
template<>
long JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return env->GetStaticLongField(classId, fieldId);
}
 
template<>
jlong JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return env->GetStaticLongField(classId, fieldId);
}
 
template<>
float JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return env->GetStaticFloatField(classId, fieldId);
}
 
template<>
int JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return env->GetStaticIntField(classId, fieldId);
}
 
template<>
std::string JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return convertFromJavaObject<std::string>(getJavaStaticField<jobject>(env, classId, fieldId));
}
 
template<>
JniObject JniObject::getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId)
{
    return convertFromJavaObject<JniObject>(getJavaStaticField<jobject>(env, classId, fieldId));
}
 
template<>
jobject JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return env->GetObjectField(objId, fieldId);
}
 
template<>
double JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return env->GetDoubleField(objId, fieldId);
}
 
template<>
long JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return env->GetLongField(objId, fieldId);
}
 
template<>
float JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return env->GetFloatField(objId, fieldId);
}
 
template<>
int JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return env->GetIntField(objId, fieldId);
}
 
template<>
std::string JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return convertFromJavaObject<std::string>(getJavaField<jobject>(env, objId, fieldId));
}
 
template<>
JniObject JniObject::getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId)
{
    return convertFromJavaObject<JniObject>(getJavaField<jobject>(env, objId, fieldId));
}
 
template<>
jarray JniObject::createJavaArray(JNIEnv* env, const jobject& element, size_t size)
{
    jclass elmClass = env->GetObjectClass(element);
    return env->NewObjectArray(size, elmClass, 0);
}
 
template<>
jarray JniObject::createJavaArray(JNIEnv* env, const double& element, size_t size)
{
    return env->NewDoubleArray(size);
}
 
template<>
jarray JniObject::createJavaArray(JNIEnv* env, const long& element, size_t size)
{
    return env->NewLongArray(size);
}

template<>
jarray JniObject::createJavaArray(JNIEnv* env, const float& element, size_t size)
{
    return env->NewFloatArray(size);
}
 
template<>
jarray JniObject::createJavaArray(JNIEnv* env, const int& element, size_t size)
{
    return env->NewLongArray(size);
}

template<>
jarray JniObject::createJavaArray(JNIEnv* env, const uint8_t& element, size_t size)
{
    return env->NewByteArray(size);
}
 
template<>
jarray JniObject::createJavaArray(JNIEnv* env, const std::string& element, size_t size)
{
    jclass elmClass = env->FindClass("java/lang/String");
    return env->NewObjectArray(size, elmClass, 0);
}
 
template<>
jarray JniObject::createJavaArray(JNIEnv* env, const JniObject& element, size_t size)
{
    jclass elmClass = element.getClass();
    return env->NewObjectArray(size, elmClass, 0);
}
 
template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, jobject& out)
{
    out = env->GetObjectArrayElement((jobjectArray)arr, position);
    return true;
}
 
template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, double& out)
{
    env->GetDoubleArrayRegion((jdoubleArray)arr, position, 1, &out);
    return true;
}
 
template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, long& out)
{
    jlong jout = 0;
    env->GetLongArrayRegion((jlongArray)arr, position, 1, &jout);
    out = jout;
    return true;
}

template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, float& out)
{
    env->GetFloatArrayRegion((jfloatArray)arr, position, 1, &out);
    return true;
}
 
template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, int& out)
{
    env->GetIntArrayRegion((jintArray)arr, position, 1, &out);
    return true;
}

template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, uint8_t& out)
{
    jbyte byte;
    env->GetByteArrayRegion((jbyteArray)arr, position, 1, &byte);
    out = (uint8_t)byte;
    return true;
}
 
template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, std::string& out)
{
    jobject obj;
    if(!convertFromJavaArrayElement(env, arr, position, obj))
    {
        return false;
    }
    convertFromJavaObject(env, obj, out);
    return true;
}
 
template<>
bool JniObject::convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, JniObject& out)
{
    jobject obj;
    if(!convertFromJavaArrayElement(env, arr, position, obj))
    {
        return false;
    }
    convertFromJavaObject(env, obj, out);
    return true;
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const jobject& elm)
{
    env->SetObjectArrayElement((jobjectArray)arr, position, elm);
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const double& elm)
{
    env->SetDoubleArrayRegion((jdoubleArray)arr, position, 1, &elm);
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const long& elm)
{
    jlong jelm = elm;
    env->SetLongArrayRegion((jlongArray)arr, position, 1, &jelm);
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const float& elm)
{
    env->SetFloatArrayRegion((jfloatArray)arr, position, 1, &elm);
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const int& elm)
{
    env->SetIntArrayRegion((jintArray)arr, position, 1, &elm);
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const uint8_t& elm)
{
    env->SetByteArrayRegion((jbyteArray)arr, position, 1, (const jbyte*)&elm);
}

template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const std::string& elm)
{
    jobject obj = env->NewStringUTF(elm.c_str());
    setJavaArrayElement(env, arr, position, obj);
}
 
template<>
void JniObject::setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const JniObject& elm)
{
    setJavaArrayElement(env, arr, position, elm.getInstance());
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<double>& obj)
{
    env->SetDoubleArrayRegion((jdoubleArray)arr, 0, obj.size(), obj.data());
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<long>& obj)
{
    env->SetLongArrayRegion((jlongArray)arr, 0, obj.size(), (const jlong*)obj.data());
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<float>& obj)
{
    env->SetFloatArrayRegion((jfloatArray)arr, 0, obj.size(), obj.data());
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<int>& obj)
{
    env->SetIntArrayRegion((jintArray)arr, 0, obj.size(), obj.data());
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<uint8_t>& obj)
{
    env->SetByteArrayRegion((jbyteArray)arr, 0, obj.size(), (const jbyte*)obj.data());
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<JniObject>& obj)
{
    size_t i = 0;
    for(auto itr = obj.begin(); itr != obj.end(); ++itr)
    {
        setJavaArrayElement(env, arr, i, *itr);
        i++;
    }
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<std::string>& obj)
{
    size_t i = 0;
    for(auto itr = obj.begin(); itr != obj.end(); ++itr)
    {
        setJavaArrayElement(env, arr, i, *itr);
        i++;
    }
}

template<>
void JniObject::setJavaArrayElements(JNIEnv* env, jarray arr, const std::vector<jobject>& obj)
{
    size_t i = 0;
    for(auto itr = obj.begin(); itr != obj.end(); ++itr)
    {
        setJavaArrayElement(env, arr, i, *itr);
        i++;
    }
}

