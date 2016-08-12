#ifndef __JniObject__
#define __JniObject__
 
#include <jni.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <array>
#include <list>
#include <set>
#include <cassert>
#include <exception>
#include <android/log.h>

class JniException: public std::exception
{
private:
    std::string _msg;
public:
    JniException(const std::string& msg):
    _msg(msg)
    {
    }

    virtual const char* what() const throw()
    {
        return _msg.c_str();
    }
};
 
class Jni
{
private:
    typedef std::map<std::string, jclass> ClassMap;
    static JavaVM* _java;
    static JNIEnv* _env;
    static int _thread;
    ClassMap _classes;
 
    Jni();
    Jni(const Jni& other);

    static void detachCurrentThread(void*);
public:
    ~Jni();
 
    /**
     * This class is a singleton
     */
    static Jni& get();
 
    /**
     * Call in the JNI_OnLoad function
     */
    jint onLoad(JavaVM* java);
 
    /**
     * Get the java virtual machine pointer
     */
    JavaVM* getJava();
 
    /**
     * Get the java environment pointer
     * Will attatch to the current thread automatically
     */
    JNIEnv* getEnvironment();
 
    /**
     * get a class, will be stored in the class cache
     */
    jclass getClass(const std::string& classPath, bool cache=true);

};
 
/**
 * This class represents a jni object
 */
class JniObject
{
private:
 
    jclass _class;
    jobject _instance;
    mutable std::string _classPath;

    static void checkJniException();
 
    template<typename Arg, typename... Args>
    static void buildSignature(std::ostringstream& os, const Arg& arg, const Args&... args)
    {
        os << getSignaturePart(arg);
        buildSignature(os, args...);
    }
 
    static void buildSignature(std::ostringstream& os)
    {
    }
 
    template<typename Return, typename... Args>
    static std::string createSignature(const Return& ret, const Args&... args)
    {
        std::ostringstream os;
        os << "(";
        buildSignature(os, args...);
        os << ")" << getSignaturePart(ret);
        return os.str();
    }
 
    template<typename... Args>
    static std::string createVoidSignature(const Args&... args)
    {
        std::ostringstream os;
        os << "(";
        buildSignature(os, args...);
        os << ")" << getSignaturePart();
        return os.str();
    }
 
    template<typename... Args>
    static jvalue* createArguments(const Args&... args)
    {
        jvalue* jargs = (jvalue*)malloc(sizeof(jvalue)*sizeof...(Args));
        buildArguments(jargs, 0, args...);
        return jargs;
    }
 
    static jvalue* createArguments()
    {
        return nullptr;
    }
 
    template<typename Arg, typename... Args>
    static void buildArguments(jvalue* jargs, unsigned pos, const Arg& arg, const Args&... args)
    {
        jargs[pos] = convertToJavaValue(arg);
        buildArguments(jargs, pos+1, args...);
        
    }
 
    static void buildArguments(jvalue* jargs, unsigned pos)
    {
    }

template<typename... Args>
    static void findObjectArguments(std::vector<jobject*>& jobjs, jvalue* jargs, const Args&... args)
    {
        processObjectArguments(jobjs, jargs, 0, args...);
    }

    template<typename Arg, typename... Args>
    static void processObjectArguments(std::vector<jobject*>& jobjs, jvalue* jargs, unsigned pos, const Arg& arg, const Args&... args)
    {
        if(isObjectArgument(arg))
        {
            jobjs.push_back(&jargs[pos].l);
        }

        processObjectArguments(jobjs, jargs, pos+1, args...);
    }

    static void processObjectArguments(std::vector<jobject*>& jobjs, jvalue* jargs, unsigned pos)
    {
    }
    
    /**
     * Return the signature for the given type
     */
    template<typename Type>
    static std::string getSignaturePart(const Type& type);
 
    /**
    * Return the signature for the given container element
    */
    template<typename Type>
    static std::string getContainerElementSignaturePart(const Type& container)
    {
        if(container.empty())
        {
            return getSignaturePart(typename Type::value_type());
        }
        else
        {
            return getSignaturePart(*container.begin());
        }
    }
 
    // template specialization for pointers
    template<typename Type>
    static std::string getSignaturePart(Type* val)
    {
        return getSignaturePart((jlong)val);
    }
 
    // template specialization for containers
    template<typename Type>
    static std::string getSignaturePart(const std::vector<Type>& val)
    {
        return std::string("[")+getContainerElementSignaturePart(val);
    }
    template<typename Type>
    static std::string getSignaturePart(const std::set<Type>& val)
    {
        return std::string("[")+getContainerElementSignaturePart(val);
    }
    template<typename Type, int Size>
    static std::string getSignaturePart(const std::array<Type, Size>& val)
    {
        return std::string("[")+getContainerElementSignaturePart(val);
    }
    template<typename Type>
    static std::string getSignaturePart(const std::list<Type>& val)
    {
        return std::string("[")+getContainerElementSignaturePart(val);
    }
    template<typename Key, typename Value>
    static std::string getSignaturePart(const std::map<Key, Value>& val)
    {
        return "Ljava/util/Map;";
    }
 
    /**
     * Return the signature for the void type
     */
    static std::string getSignaturePart();
 
    template<typename Return>
    Return callStaticJavaMethod(JNIEnv* env, jclass classId, jmethodID methodId, jvalue* args);
 
    void callJavaVoidMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args);

    template<typename Return>
    void callJavaObjectMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, Return& out)
    {
        jobject jout = nullptr;
        callJavaMethod(env, objId, methodId, args, jout);
        checkJniException();
        out = convertFromJavaObject<Return>(jout);
    }

    template<typename Return>
    void callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, Return& out)
    {
        callJavaObjectMethod(env, objId, methodId, args, out);
    }

    template<typename Type>
    void callJavaMethod(JNIEnv* env, jobject objId, jmethodID methodId, jvalue* args, std::vector<Type>& out)
    {
        callJavaObjectMethod(env, objId, methodId, args, out);
    }
 
    template<typename Return>
    Return getJavaStaticField(JNIEnv* env, jclass classId, jfieldID fieldId);
 
    template<typename Return>
    Return getJavaField(JNIEnv* env, jobject objId, jfieldID fieldId);
 
public:
    JniObject(const std::string& classPath, jobject javaObj=nullptr, jclass classId=nullptr);
    JniObject(jclass classId, jobject javaObj);
    JniObject(jobject javaObj=nullptr);
    JniObject(const JniObject& other);
    void init(jobject javaObj=nullptr, jclass classId=nullptr, const std::string& classPath="");
    ~JniObject();
 
    /**
     * Clear the retained global references
     */
    void clear();
 
    /**
     * Find a singleton instance
     * will try the `instance` static field and a `getInstance` static method
     */
    static JniObject findSingleton(const std::string& classPath);
 
    /**
     * Create a new JniObject
     */
    template<typename... Args>
    static JniObject createNew(const std::string& classPath, Args&&... args)
    {
        JniObject defRet(classPath);
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            return defRet;
        }
        jclass classId = Jni::get().getClass(classPath);
        if(!classId)
        {
            return defRet;
        }
        std::string signature(createVoidSignature<Args...>(args...));
        jmethodID methodId = env->GetMethodID(classId, "<init>", signature.c_str());
        checkJniException();
        jvalue* jargs = createArguments(args...);
        jobject obj = env->NewObjectA(classId, methodId, jargs);
        checkJniException();
        defRet = JniObject(classPath, obj, classId);
        return defRet;
    }

    template<typename... Args>
    void cleanupArguments(JNIEnv* env, jvalue* jargs, Args&&... args)
    {
        std::vector<jobject*> jobjs;
        findObjectArguments(jobjs, jargs, args...);
        for(jobject* jobj : jobjs)
        {
            env->DeleteLocalRef(*jobj);
        }
    }
 
    /**
     * Calls an object method
     */
    template<typename Return, typename... Args>
    Return call(const std::string& name, const Return& defRet, Args&&... args)
    {
        std::string signature(createSignature(defRet, args...));
        return callSigned(name, signature, defRet, args...);
    }
 
    template<typename Return, typename... Args>
    Return callSigned(const std::string& name, const std::string& signature, const Return& defRet, Args&&... args)
    {
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            throw JniException("no environment found");
        }
        jclass classId = getClass();
        if(!classId)
        {
            throw JniException("no class found");
        }
        jobject objId = getInstance();
        if(!objId)
        {
            throw JniException("no object found");
        }
        jmethodID methodId = env->GetMethodID(classId, name.c_str(), signature.c_str());
        checkJniException();
        jvalue* jargs = createArguments(args...);
        Return result;
        callJavaMethod(env, objId, methodId, jargs, result);
        cleanupArguments(env, jargs, args...);
        checkJniException();
        return result;
    }
 
    /**
     * Calls an object void method
     */
    template<typename... Args>
    void callVoid(const std::string& name, Args&&... args)
    {
        std::string signature(createVoidSignature(args...));
        return callSignedVoid(name, signature, args...);
    }
 
    template<typename... Args>
    void callSignedVoid(const std::string& name, const std::string& signature, Args&&... args)
    {
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            throw JniException("no environment found");
        }
        jclass classId = getClass();
        if(!classId)
        {
            throw JniException("no class found");
        }
        jobject objId = getInstance();
        if(!objId)
        {
            throw JniException("no object found");
        }
        jmethodID methodId = env->GetMethodID(classId, name.c_str(), signature.c_str());
        checkJniException();
        jvalue* jargs = createArguments(args...);
        callJavaVoidMethod(env, objId, methodId, jargs);
        cleanupArguments(env, jargs, args...);
        checkJniException();
    }
 
    /**
     * Calls a class method
     */
    template<typename Return, typename... Args>
    Return staticCall(const std::string& name, const Return& defRet, Args&&... args)
    {
        std::string signature(createSignature(defRet, args...));
        return staticCallSigned(name, signature, defRet, args...);
    }
 
    template<typename Return, typename... Args>
    Return staticCallSigned(const std::string& name, const std::string& signature, const Return& defRet, Args&&... args)
    {
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            throw JniException("no environment found");
        }
        jclass classId = getClass();
        if(!classId)
        {
            throw JniException("no class found");
        }
        jmethodID methodId = env->GetStaticMethodID(classId, name.c_str(), signature.c_str());
        checkJniException();
        jvalue* jargs = createArguments(args...);
        Return result = callStaticJavaMethod<Return>(env, classId, methodId, jargs);
        cleanupArguments(env, jargs, args...);
        checkJniException();
        return result;
    }
 
    /**
     * Calls a class void method
     */
    template<typename... Args>
    void staticCallVoid(const std::string& name, Args&&... args)
    {
        std::string signature(createVoidSignature(args...));
        return staticCallSignedVoid(name, signature, args...);
    }
 
    template<typename... Args>
    void staticCallSignedVoid(const std::string& name, const std::string& signature, Args&&... args)
    {
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            throw JniException("no environment found");
        }
        jclass classId = getClass();
        if(!classId)
        {
            throw JniException("no class found");
        }
        jmethodID methodId = env->GetStaticMethodID(classId, name.c_str(), signature.c_str());
        checkJniException();
        jvalue* jargs = createArguments(args...);
        callStaticJavaMethod<void>(env, classId, methodId, jargs);
        cleanupArguments(env, jargs, args...);
        checkJniException();
    }
 
    /**
     * Get a static class field
     * @param name the field name
     */
    template<typename Return>
    Return staticField(const std::string& name, const Return& defRet)
    {
        std::string signature(getSignaturePart<Return>(defRet));
        return staticFieldSigned(name, signature, defRet);
    }
 
    template<typename Return>
    Return staticFieldSigned(const std::string& name, const std::string& signature, const Return& defRet)
    {
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            throw JniException("no environment found");
        }
 
        jclass classId = getClass();
        if(!classId)
        {
            throw JniException("no class found");
        }
 
        jfieldID fieldId = env->GetStaticFieldID(classId, name.c_str(), signature.c_str());
        checkJniException();
        Return result = getJavaStaticField<Return>(env, classId, fieldId);
        checkJniException();
        return result;
    }
 
    /**
     * Get a object field
     * @param name the field name
     */
    template<typename Return>
    Return field(const std::string& name, const Return& defRet)
    {
        std::string signature(getSignaturePart<Return>(defRet));
        return fieldSigned(name, signature, defRet);
    }
 
    template<typename Return>
    Return fieldSigned(const std::string& name, const std::string& signature, const Return& defRet)
    {
        JNIEnv* env = getEnvironment();
        if(!env)
        {
            throw JniException("no environment found");
        }
 
        jclass classId = getClass();
        if(!classId)
        {
            throw JniException("no class found");
        }
 
        jfieldID fieldId = env->GetFieldID(classId, name.c_str(), signature.c_str());
        checkJniException();
        Return result = getJavaField<Return>(env, classId, fieldId);
        checkJniException();        
        return result;
    }
 
    /**
     * Return the signature for the object
     */
    std::string getSignature() const;

    /**
     * create an java array of the given type
     */
    template<typename Type>
    static jarray createJavaArray(JNIEnv* env, const Type& element, size_t size);
 
    /**
     * Convert a jobject array to a container
     */
    template<typename Type>
    static bool convertFromJavaArray(JNIEnv* env, jarray arr, Type& container)
    {
        if(!arr)
        {
            return false;
        }
        jsize arraySize = env->GetArrayLength(arr);
        for(size_t i=0; i<arraySize; i++)
        {
            typename Type::value_type elm;
            convertFromJavaArrayElement(env, arr, i, elm);
            container.insert(container.end(), elm);
        }
        return true;
    }
 
    template<typename Type>
    static bool convertFromJavaArray(jarray arr, Type& container)
    {
        JNIEnv* env = getEnvironment();
        assert(env);
        return convertFromJavaArray(env, arr, container);
    }
 
    /**
     * Get an element of a java array
     */
    template<typename Type>
    static bool convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, Type& out);
 
    template<typename Type>
    static bool convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, std::vector<Type>& out)
    {
        jobject elm;
        if(!convertFromJavaArrayElement(env, arr, position, elm))
        {
            return false;
        }
        return convertFromJavaArray(env, (jarray)elm, out);
    }
 
    template<typename Key, typename Value>
    static bool convertFromJavaArrayElement(JNIEnv* env, jarray arr, size_t position, std::map<Key, Value>& out)
    {
        jobject elm;
        if(!convertFromJavaArrayElement(env, arr, position, elm))
        {
            return false;
        }
        return convertFromJavaMap(env, elm, out);
    }
 
    /**
    * Set an element of a java array
    */
    template<typename Type>
    static void setJavaArrayElement(JNIEnv* env, jarray arr, size_t position, const Type& elm);

    /**
    * Set all elements of a java array
    */
    template<typename Type>
    static void setJavaArrayElements(JNIEnv* env, jarray arr, const Type& obj);
 
    template<typename Type>
    static bool convertFromJavaCollection(JNIEnv* env, jobject obj, Type& out)
    {
        if(!obj)
        {
            return false;
        }
        try
        {
            JniObject jcontainer(obj);
            if(!jcontainer.isInstanceOf("java.util.Collection"))
            {
                return false;
            }
            out = jcontainer.call<Type>("toArray", out, out);
            return true;            
        }
        catch(JniException)
        {
            return false;
        }
    }
 
    template<typename Key, typename Value>
    static bool convertFromJavaMap(JNIEnv* env, jobject obj, std::map<Key, Value>& out)
    {
        if(!obj)
        {
            return false;
        }
        JniObject jmap(obj);
        if(!jmap.isInstanceOf("java.util.Map"))
        {
            return false;
        }
        JniObject jkeys = jmap.call<JniObject>("keySet", JniObject("java.util.Set"));
        std::vector<Key> keys = jkeys.callSigned<std::vector<Key>>("toArray", "()[Ljava/lang/Object;", std::vector<Key>());
        for(typename std::vector<Key>::const_iterator itr = keys.begin(); itr != keys.end(); ++itr)
        {
            Value v = jmap.callSigned<Value>("get", "(Ljava/lang/Object;)Ljava/lang/Object;", Value(), *itr);
            out[*itr] = v;
        }
        return true;
    }
 
    template<typename Key, typename Value>
    static bool convertToMapFromJavaArray(JNIEnv* env, jarray arr, std::map<Key, Value>& out)
    {
        if(!arr)
        {
            return false;
        }
        jsize mapSize = env->GetArrayLength(arr) / 2;
        for(size_t i=0; i<mapSize; ++i)
        {
            Key k;
            if(convertFromJavaArrayElement(env, arr, i*2, k))
            {
                Value v;
                if(convertFromJavaArrayElement(env, arr, i*2+1, v))
                {
                    out[k] = v;
                }
            }
        }
        return true;
    }
 
    /**
     * Convert a jobject to the c++ representation
     */
    template<typename Type>
    static bool convertFromJavaObject(JNIEnv* env, jobject obj, Type& out);
 
    // template specialization for containers
    template<typename Type>
    static bool convertFromJavaObject(JNIEnv* env, jobject obj, std::vector<Type>& out)
    {
        if(convertFromJavaCollection(env, obj, out))
        {
            return true;
        }
        if(convertFromJavaArray(env, (jarray)obj, out))
        {
            return true;
        }
        return false;
    }
    template<typename Type>
    static bool convertFromJavaObject(JNIEnv* env, jobject obj, std::set<Type>& out)
    {
        if(convertFromJavaCollection(env, obj, out))
        {
            return true;
        }
        if(convertFromJavaArray(env, (jarray)obj, out))
        {
            return true;
        }
        return false;
    }
    template<typename Type, int Size>
    static bool convertFromJavaObject(JNIEnv* env, jobject obj, std::array<Type, Size>& out)
    {
        if(convertFromJavaCollection(env, obj, out))
        {
            return true;
        }
        if(convertFromJavaArray(env, (jarray)obj, out))
        {
            return true;
        }
        return false;
    }
    template<typename Type>
    static bool convertFromJavaObject(JNIEnv* env, jobject obj, std::list<Type>& out)
    {
        if(convertFromJavaCollection(env, obj, out))
        {
            return true;
        }
        if(convertFromJavaArray(env, (jarray)obj, out))
        {
            return true;
        }
        return false;
    }
 
    template<typename Key, typename Type>
    static bool convertFromJavaObject(JNIEnv* env, jobject obj, std::map<Key, Type>& out)
    {
        if(convertFromJavaMap(env, obj, out))
        {
            return true;
        }
        if(convertToMapFromJavaArray(env, (jarray)obj, out))
        {
            return true;
        }
        return false;
    }
 
 
    // utility methods that return the object
    template<typename Type>
    static Type convertFromJavaObject(JNIEnv* env, jobject obj)
    {
        Type out;
        bool result = convertFromJavaObject(env, obj, out);
        assert(result);
        return out;
    }
    template<typename Type>
    static Type convertFromJavaObject(jobject obj)
    {
        JNIEnv* env = getEnvironment();
        assert(env);
        return convertFromJavaObject<Type>(env, obj);
    }
    template<typename Type>
    static bool convertFromJavaObject(jobject obj, Type& out)
    {
        JNIEnv* env = getEnvironment();
        assert(env);
        return convertFromJavaObject(env, obj, out);
    }
 
    /**
     * Convert a c++ list container to a jarray
     */
    template<typename Type>
    static jarray createJavaArray(const Type& obj)
    {
        JNIEnv* env = getEnvironment();
        if (!env)
        {
            return nullptr;
        }
        jarray arr = nullptr;
        if(obj.empty())
        {
            arr = createJavaArray(env, typename Type::value_type(), 0);
        }
        else
        {
            arr = createJavaArray(env, *obj.begin(), obj.size());
        }
        setJavaArrayElements(env, arr, obj);
        return arr;
    }
 
    template<typename Key, typename Value>
    static JniObject createJavaMap(const std::map<Key, Value>& obj, const std::string& classPath="java/util/HashMap")
    {
        JniObject jmap(JniObject::createNew(classPath));
        for(typename std::map<Key, Value>::const_iterator itr = obj.begin(); itr != obj.end(); ++itr)
        {
        	Value v = jmap.callSigned("put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", Value(), itr->first, itr->second);
        }
        return jmap;
    }
 
    template<typename Type>
    static JniObject createJavaList(const Type& obj, const std::string& classPath="java/util/ArrayList")
    {
        JniObject jlist(JniObject::createNew(classPath));
        for(typename Type::const_iterator itr = obj.begin(); itr != obj.end(); ++itr)
        {
            jlist.callSignedVoid("add", "(Ljava/lang/Object;)Z", *itr);
        }
        return jlist;
    }
 
    template<typename Type>
    static JniObject createJavaSet(const Type& obj, const std::string& classPath="java/util/HashSet")
    {
        return createJavaList(obj, classPath);
    }
 
    /**
     * Convert a c++ type to the jvalue representation
     * This is called on all jni arguments
     */
    template<typename Type>
    static jvalue convertToJavaValue(const Type& obj);
 
    // template specialization for pointers
    template<typename Type>
    static jvalue convertToJavaValue(Type* obj)
    {
        return convertToJavaValue((jlong)obj);
    }
 
    // template specialization for containers
    template<typename Type>
    static jvalue convertToJavaValue(const std::vector<Type>& obj)
    {
        return convertToJavaValue(createJavaArray(obj));
    }
    template<typename Type>
    static jvalue convertToJavaValue(const std::set<Type>& obj)
    {
        return convertToJavaValue(createJavaArray(obj));
    }
    template<typename Type, int Size>
    static jvalue convertToJavaValue(const std::array<Type, Size>& obj)
    {
        return convertToJavaValue(createJavaArray(obj));
    }
    template<typename Type>
    static jvalue convertToJavaValue(const std::list<Type>& obj)
    {
        return convertToJavaValue(createJavaArray(obj));
    }
    template<typename Key, typename Value>
    static jvalue convertToJavaValue(const std::map<Key, Value>& obj)
    {
        return convertToJavaValue(createJavaMap(obj).getNewLocalInstance());
    }

    /**
     * Return if the argument is a java object
     * This is called on all jni arguments
     * to decide if we need to delete a local ref
     */
    template<typename Type>
    static bool isObjectArgument(const Type& obj);

    // template specialization for pointers
    template<typename Type>
    static bool isObjectArgument(Type* obj)
    {
        return false;
    }

    // template specialization for containers
    template<typename Type>
    static bool isObjectArgument(const std::vector<Type>& obj)
    {
        return true;
    }

    template<typename Type>
    static bool isObjectArgument(const std::set<Type>& obj)
    {
        return true;
    }

    template<typename Type, int Size>
    static bool isObjectArgument(const std::array<Type, Size>& obj)
    {
        return true;
    }

    template<typename Type>
    static bool isObjectArgument(const std::list<Type>& obj)
    {
        return true;
    }

    template<typename Key, typename Value>
    static bool isObjectArgument(const std::map<Key, Value>& obj)
    {
        return true;
    }
 
    /**
     * Returns the class reference. This is a global ref that will be removed
     * when the JniObject is destroyed
     */
    jclass getClass() const;
 
    /**
     * Returns the class path. If it is not there it tries to call
     * `getClass().getName()` on the object to get the class
     */
    const std::string& getClassPath() const;
 
    /**
     * Returns the jobject reference. This is a global ref that will be removed
     * when the JniObject is destroyed
     */
    jobject getInstance() const;
 
    /**
    * Returns the jobject reference. This is a new local ref
    */
    jobject getNewLocalInstance() const;
 
    /**
     * Return true if class path and class ref match
     */
    bool isInstanceOf(const std::string& classPath) const;
 
    /**
     * Returns the environment pointer
     */
    static JNIEnv* getEnvironment();
 
    /**
     * Returns true if there is an object instance
     */
    operator bool() const;
 
    /**
     * Copy a jni object
     */
    JniObject& operator=(const JniObject& other);
 
    /**
     * Compare two jni objects
     */
    bool operator==(const JniObject& other) const;
};
 
#endif
