#ifndef _DALVIK_REFLECT_REFLECT
#define _DALVIK_REFLECT_REFLECT

bool dvmReflectStartup(void);
bool dvmReflectProxyStartup(void);
bool dvmReflectAnnotationStartup(void);
void dvmReflectShutdown(void);

bool dvmValidateBoxClasses();

ArrayObject* dvmGetDeclaredFields(ClassObject* clazz, bool publicOnly);

ArrayObject* dvmGetDeclaredConstructors(ClassObject* clazz, bool publicOnly);

ArrayObject* dvmGetDeclaredMethods(ClassObject* clazz, bool publicOnly);

ArrayObject* dvmGetInterfaces(ClassObject* clazz);

Field* dvmSlotToField(ClassObject* clazz, int slot);
Method* dvmSlotToMethod(ClassObject* clazz, int slot);

int dvmConvertPrimitiveValue(PrimitiveType srcType,
    PrimitiveType dstType, const s4* srcPtr, s4* dstPtr);

int dvmConvertArgument(DataObject* arg, ClassObject* type, s4* ins);

DataObject* dvmWrapPrimitive(JValue value, ClassObject* returnType);

bool dvmUnwrapPrimitive(Object* value, ClassObject* returnType,
    JValue* pResult);

ClassObject* dvmGetBoxedReturnType(const Method* meth);

Field* dvmGetFieldFromReflectObj(Object* obj);
Method* dvmGetMethodFromReflectObj(Object* obj);
Object* dvmCreateReflectObjForField(const ClassObject* clazz, Field* field);
Object* dvmCreateReflectObjForMethod(const ClassObject* clazz, Method* method);

INLINE bool dvmIsReflectionMethod(const Method* method)
{
    return (method->clazz == gDvm.classJavaLangReflectMethod);
}

ClassObject* dvmGenerateProxyClass(StringObject* str, ArrayObject* interfaces,
    Object* loader);

Object* dvmCreateReflectMethodObject(const Method* meth);

ArrayObject* dvmGetClassAnnotations(const ClassObject* clazz);
ArrayObject* dvmGetMethodAnnotations(const Method* method);
ArrayObject* dvmGetFieldAnnotations(const Field* field);
ArrayObject* dvmGetParameterAnnotations(const Method* method);

Object* dvmGetAnnotationDefaultValue(const Method* method);

ArrayObject* dvmGetMethodThrows(const Method* method);
Object* dvmGetClassSignatureAnnotation(const ClassObject* clazz);
ArrayObject* dvmGetMethodSignatureAnnotation(const Method* method);
ArrayObject* dvmGetFieldSignatureAnnotation(const Field* field);

Object* dvmGetEnclosingMethod(const ClassObject* clazz);

ClassObject* dvmGetDeclaringClass(const ClassObject* clazz);

ClassObject* dvmGetEnclosingClass(const ClassObject* clazz);

bool dvmGetInnerClass(const ClassObject* clazz, StringObject** pName,
    int* pAccessFlags);

ArrayObject* dvmGetDeclaredClasses(const ClassObject* clazz);

typedef struct AnnotationValue {
    JValue  value;
    u1      type;
} AnnotationValue;


typedef struct {
    const u1* cursor;                    /* current cursor */
    u4 elementsLeft;                     /* number of elements left to read */
    const DexEncodedArray* encodedArray; /* instance being iterated over */
    u4 size;                             /* number of elements in instance */
    const ClassObject* clazz;            /* class to resolve with respect to */
} EncodedArrayIterator;

void dvmEncodedArrayIteratorInitialize(EncodedArrayIterator* iterator,
        const DexEncodedArray* encodedArray, const ClassObject* clazz);

bool dvmEncodedArrayIteratorHasNext(const EncodedArrayIterator* iterator);

bool dvmEncodedArrayIteratorGetNext(EncodedArrayIterator* iterator,
        AnnotationValue* value);

#endif
