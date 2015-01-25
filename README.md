JniObject
=========

JniObject is a C++11 class that simplifyies the communication between
C++ and java using the jni library.


Using jni directly to call a method and pass a string argument is something
like this:

```c++
JNIEnv* env; 
java->GetEnv(&env, JNI_VERSION_1_4);
jobject obj;
jclass cls = env->GetObjectClass(env, obj);
jmethodID mid = env->GetMethodID(env, cls, "MethodName", "(Ljava/lang/String;)V");
jstring javaString = env->NewStringUTF(nativeString.c_str());
env->CallVoidMethod(obj, mid, javaString);
env->DeleteGlobalRef(cls);
```

With `JniObject` it's as simple as this:

```c++
JniObject obj;
obj.callVoid("MethodName", nativeString);
```

Some of the features of the class include:
* automatic method signature deduction
* automatic singleton references
* java object creation
* methods that return other java objects
* multithreading support
* conversion from `std::string` to java `String`
* conversion from `std::vector` to and from java arrays and `List`
* conversion from `std::map` to and from java `Map`

Read [this blog post](http://engineering.socialpoint.es/cpp-wrapper-for-jni.html)
for a more in depth look into it.
