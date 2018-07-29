
#include "lottieparser.h"

#define DEBUG_PARSER

// This parser implements JSON token-by-token parsing with an API that is
// more direct; we don't have to create  handler object and
// callbacks. Instead, we retrieve values from the JSON stream by calling
// GetInt(), GetDouble(), GetString() and GetBool(), traverse into structures
// by calling EnterObject() and EnterArray(), and skip over unwanted data by
// calling SkipValue(). As we know the lottie file structure this way will be the efficient way
// of parsing the file.
//
// If you aren't sure of what's next in the JSON data, you can use PeekType() and
// PeekValue() to look ahead to the next object before reading it.
//
// If you call the wrong retrieval method--e.g. GetInt when the next JSON token is
// not an int, EnterObject or EnterArray when there isn't actually an object or array
// to read--the stream parsing will end immediately and no more data will be delivered.
//
// After calling EnterObject, you retrieve keys via NextObjectKey() and values via
// the normal getters. When NextObjectKey() returns null, you have exited the
// object, or you can call SkipObject() to skip to the end of the object
// immediately. If you fetch the entire object (i.e. NextObjectKey() returned  null),
// you should not call SkipObject().
//
// After calling EnterArray(), you must alternate between calling NextArrayValue()
// to see if the array has more data, and then retrieving values via the normal
// getters. You can call SkipArray() to skip to the end of the array immediately.
// If you fetch the entire array (i.e. NextArrayValue() returned null),
// you should not call SkipArray().
//
// This parser uses in-situ strings, so the JSON buffer will be altered during the
// parse.


#include "rapidjson/document.h"
#include <iostream>
#include "lottiemodel.h"
#include"velapsedtimer.h"


RAPIDJSON_DIAG_PUSH
#ifdef __GNUC__
RAPIDJSON_DIAG_OFF(effc++)
#endif

using namespace rapidjson;

class LookaheadParserHandler {
public:
    bool Null() { st_ = kHasNull; v_.SetNull(); return true; }
    bool Bool(bool b) { st_ = kHasBool; v_.SetBool(b); return true; }
    bool Int(int i) { st_ = kHasNumber; v_.SetInt(i); return true; }
    bool Uint(unsigned u) { st_ = kHasNumber; v_.SetUint(u); return true; }
    bool Int64(int64_t i) { st_ = kHasNumber; v_.SetInt64(i); return true; }
    bool Uint64(uint64_t u) { st_ = kHasNumber; v_.SetUint64(u); return true; }
    bool Double(double d) { st_ = kHasNumber; v_.SetDouble(d); return true; }
    bool RawNumber(const char*, SizeType, bool) { return false; }
    bool String(const char* str, SizeType length, bool) { st_ = kHasString; v_.SetString(str, length); return true; }
    bool StartObject() { st_ = kEnteringObject; return true; }
    bool Key(const char* str, SizeType length, bool) { st_ = kHasKey; v_.SetString(str, length); return true; }
    bool EndObject(SizeType) { st_ = kExitingObject; return true; }
    bool StartArray() { st_ = kEnteringArray; return true; }
    bool EndArray(SizeType) { st_ = kExitingArray; return true; }

protected:
    LookaheadParserHandler(char* str);
    void ParseNext();

protected:
    enum LookaheadParsingState {
        kInit,
        kError,
        kHasNull,
        kHasBool,
        kHasNumber,
        kHasString,
        kHasKey,
        kEnteringObject,
        kExitingObject,
        kEnteringArray,
        kExitingArray
    };

    Value v_;
    LookaheadParsingState st_;
    Reader r_;
    InsituStringStream ss_;

    static const int parseFlags = kParseDefaultFlags | kParseInsituFlag;
};


class LottieParserImpl : protected LookaheadParserHandler {
public:
    LottieParserImpl(char* str) : LookaheadParserHandler(str) {}

public:
    bool EnterObject();
    bool EnterArray();
    const char* NextObjectKey();
    bool NextArrayValue();
    int GetInt();
    double GetDouble();
    const char* GetString();
    bool GetBool();
    void GetNull();

    void SkipObject();
    void SkipArray();
    void SkipValue();
    Value* PeekValue();
    int PeekType(); // returns a rapidjson::Type, or -1 for no value (at end of object/array)

    bool IsValid() { return st_ != kError; }

    void Skip(const char *key);
    VRect getRect();
    LottieBlendMode getBlendMode();
    CapStyle getLineCap();
    JoinStyle getLineJoin();
    FillRule getFillRule();
    LOTTrimData::TrimType getTrimType();
    MatteType getMatteType();
    LayerType getLayerType();

    std::shared_ptr<LOTCompositionData> composition() const {return mComposition;}
    void parseComposition();
    void parseAssets(LOTCompositionData *comp);
    std::shared_ptr<LOTAsset> parseAsset();
    void parseLayers(LOTCompositionData *comp);
    std::shared_ptr<LOTData> parseLayer();
    void parseMaskProperty(LOTLayerData *layer);
    void parseShapesAttr(LOTLayerData *layer);
    void parseObject(LOTGroupData *parent);
    std::shared_ptr<LOTMaskData>  parseMaskObject();
    std::shared_ptr<LOTData> parseObjectTypeAttr();
    std::shared_ptr<LOTData> parseGroupObject();
    std::shared_ptr<LOTData> parseRectObject();
    std::shared_ptr<LOTData> parseEllipseObject();
    std::shared_ptr<LOTData> parseShapeObject();
    std::shared_ptr<LOTData> parsePolystarObject();

    std::shared_ptr<LOTTransformData> parseTransformObject();
    std::shared_ptr<LOTData> parseFillObject();
    std::shared_ptr<LOTData> parseGFillObject();
    std::shared_ptr<LOTData> parseStrokeObject();
    std::shared_ptr<LOTData> parseGStrokeObject();
    std::shared_ptr<LOTData> parseTrimObject();
    std::shared_ptr<LOTData> parseReapeaterObject();


    void parseGradientProperty(LOTGradient *gradient, const char *key);

    VPointF parseInperpolatorPoint();
    void parseArrayValue(VPointF &pt);
    void parseArrayValue(LottieColor &pt);
    void parseArrayValue(float &val);
    void parseArrayValue(int &val);
    void parseArrayValue(LottieGradient &gradient);
    void getValue(VPointF &val);
    void getValue(float &val);
    void getValue(LottieColor &val);
    void getValue(int &val);
    void getValue(LottieShapeData &shape);
    void getValue(LottieGradient &gradient);
    template<typename T>
    bool parseKeyFrameValue(const char* key, LOTKeyFrameValue<T> &value);
    template<typename T>
    void parseKeyFrame(LOTAnimInfo<T> &obj);
    template<typename T>
    void parseProperty(LOTAnimatable<T> &obj);

    void parseShapeKeyFrame(LOTAnimInfo<LottieShapeData> &obj);
    void parseShapeProperty(LOTAnimatable<LottieShapeData> &obj);
    void parseArrayValue(std::vector<VPointF> &v);
    void parseDashProperty(LOTDashProperty &dash);

    LottieColor toColor(const char *str);

    void resolveLayerRefs();
protected:
    std::shared_ptr<LOTCompositionData>            mComposition;
    LOTCompositionData                            *compRef;
    LOTLayerData                                  *curLayerRef;
    std::vector<std::shared_ptr<LOTLayerData>>     mLayersToUpdate;
    void SkipOut(int depth);
};

LookaheadParserHandler::LookaheadParserHandler(char* str) : v_(), st_(kInit), r_(), ss_(str) {
    r_.IterativeParseInit();
    ParseNext();
}

void LookaheadParserHandler::ParseNext() {
    if (r_.HasParseError()) {
        st_ = kError;
        return;
    }

    if (!r_.IterativeParseNext<parseFlags>(ss_, *this)) {
        vCritical<<"Lottie file parsing error";
        RAPIDJSON_ASSERT(0);
    }
}


bool LottieParserImpl::EnterObject() {
    if (st_ != kEnteringObject) {
        st_  = kError;
        RAPIDJSON_ASSERT(false);
        return false;
    }

    ParseNext();
    return true;
}

bool LottieParserImpl::EnterArray() {
    if (st_ != kEnteringArray) {
        st_  = kError;
        RAPIDJSON_ASSERT(false);
        return false;
    }

    ParseNext();
    return true;
}

const char* LottieParserImpl::NextObjectKey() {
    if (st_ == kHasKey) {
        const char* result = v_.GetString();
        ParseNext();
        return result;
    }

    /* SPECIAL CASE
     * The parser works with a prdefined rule that it will be only
     * while (NextObjectKey()) for each object but in case of our nested group
     * object we can call multiple time NextObjectKey() while exiting the object
     * so ignore those and don't put parser in the error state.
     * */
    if (st_ == kExitingArray || st_ == kEnteringObject ) {
// #ifdef DEBUG_PARSER
//         vDebug<<"Object: Exiting nested loop";
// #endif
        return 0;
    }

    if (st_ != kExitingObject) {
        RAPIDJSON_ASSERT(false);
        st_ = kError;
        return 0;
    }

    ParseNext();
    return 0;
}

bool LottieParserImpl::NextArrayValue() {
    if (st_ == kExitingArray) {
        ParseNext();
        return false;
    }

    /* SPECIAL CASE
     * same as  NextObjectKey()
     */
    if (st_ == kExitingObject) {
// #ifdef DEBUG_PARSER
//         vDebug<<"Array: Exiting nested loop";
// #endif
        return 0;
    }


    if (st_ == kError || st_ == kHasKey) {
        RAPIDJSON_ASSERT(false);
        st_ = kError;
        return false;
    }

    return true;
}

int LottieParserImpl::GetInt() {
    if (st_ != kHasNumber || !v_.IsInt()) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return 0;
    }

    int result = v_.GetInt();
    ParseNext();
    return result;
}

double LottieParserImpl::GetDouble() {
    if (st_ != kHasNumber) {
        st_  = kError;
        RAPIDJSON_ASSERT(false);
        return 0.;
    }

    double result = v_.GetDouble();
    ParseNext();
    return result;
}

bool LottieParserImpl::GetBool() {
    if (st_ != kHasBool) {
        st_  = kError;
        RAPIDJSON_ASSERT(false);
        return false;
    }

    bool result = v_.GetBool();
    ParseNext();
    return result;
}

void LottieParserImpl::GetNull() {
    if (st_ != kHasNull) {
        st_  = kError;
        return;
    }

    ParseNext();
}

const char* LottieParserImpl::GetString() {
    if (st_ != kHasString) {
        st_  = kError;
        RAPIDJSON_ASSERT(false);
        return 0;
    }

    const char* result = v_.GetString();
    ParseNext();
    return result;
}

void LottieParserImpl::SkipOut(int depth) {
    do {
        if (st_ == kEnteringArray || st_ == kEnteringObject) {
            ++depth;
        }
        else if (st_ == kExitingArray || st_ == kExitingObject) {
            --depth;
        }
        else if (st_ == kError) {
            RAPIDJSON_ASSERT(false);
            return;
        }

        ParseNext();
    }
    while (depth > 0);
}

void LottieParserImpl::SkipValue() {
    SkipOut(0);
}

void LottieParserImpl::SkipArray() {
    SkipOut(1);
}

void LottieParserImpl::SkipObject() {
    SkipOut(1);
}

Value* LottieParserImpl::PeekValue() {
    if (st_ >= kHasNull && st_ <= kHasKey) {
        return &v_;
    }

    return 0;
}

int LottieParserImpl::PeekType() {
    if (st_ >= kHasNull && st_ <= kHasKey) {
        return v_.GetType();
    }

    if (st_ == kEnteringArray) {
        return kArrayType;
    }

    if (st_ == kEnteringObject) {
        return kObjectType;
    }

    return -1;
}

void LottieParserImpl::Skip(const char *key)
{
    if (PeekType() == kArrayType) {
        EnterArray();
        SkipArray();
    } else if (PeekType() == kObjectType) {
        EnterObject();
        SkipObject();
    } else {
        SkipValue();
    }
}

LottieBlendMode
LottieParserImpl::getBlendMode()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    LottieBlendMode mode = LottieBlendMode::Normal;

    switch (GetInt()) {
    case 1:
        mode = LottieBlendMode::Multiply;
        break;
    case 2:
        mode = LottieBlendMode::Screen;
        break;
    case 3:
        mode = LottieBlendMode::OverLay;
        break;
    default:
        break;
    }
    return mode;
}
VRect LottieParserImpl::getRect()
{
    VRect r;
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "l")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            r.setLeft(GetInt());
        } else if (0 == strcmp(key, "r")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            r.setRight(GetInt());
        } else if (0 == strcmp(key, "t")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            r.setTop(GetInt());
        } else if (0 == strcmp(key, "b")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            r.setBottom(GetInt());
        } else {
            RAPIDJSON_ASSERT(false);
        }
    }
    return r;
}

void LottieParserImpl::resolveLayerRefs()
{
    for(auto i : mLayersToUpdate) {
        LOTLayerData *layer = i.get();
        auto search = compRef->mAssets.find(layer->mPreCompRefId);
        if (search != compRef->mAssets.end()) {
           layer->mChildren = search->second.get()->mLayers;
       }
    }
}


void
LottieParserImpl::parseComposition()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    std::shared_ptr<LOTCompositionData> sharedComposition = std::make_shared<LOTCompositionData>();
    LOTCompositionData *comp = sharedComposition.get();
    compRef = comp;
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "v")) {
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            comp->mVersion = std::string(GetString());
        } else if (0 == strcmp(key, "w")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mSize.setWidth(GetInt());
        } else if (0 == strcmp(key, "h")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mSize.setHeight(GetInt());
        } else if (0 == strcmp(key, "ip")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mStartFrame = GetDouble();
        } else if (0 == strcmp(key, "op")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mEndFrame = GetDouble();
        } else if (0 == strcmp(key, "fr")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mFrameRate = GetDouble();
        } else if (0 == strcmp(key, "assets")) {
            parseAssets(comp);
        } else if (0 == strcmp(key, "layers")) {
            parseLayers(comp);
        } else {
#ifdef DEBUG_PARSER
            vWarning<<"Composition Attribute Skipped : "<<key;
#endif
            Skip(key);
        }
    }
    resolveLayerRefs();
    // update the static property of Composition
    bool staticFlag = true;
    for (auto child : comp->mChildren) {
        staticFlag &= child.get()->isStatic();
    }
    comp->setStatic(staticFlag);

    mComposition = sharedComposition;
}

void LottieParserImpl::parseAssets(LOTCompositionData *composition)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        std::shared_ptr<LOTAsset> asset = parseAsset();
        composition->mAssets[asset->mRefId] = asset;
    }
    // update the precomp layers with the actual layer object

}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/shape.json
 *
 */
std::shared_ptr<LOTAsset>
LottieParserImpl::parseAsset()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    std::shared_ptr<LOTAsset> sharedAsset = std::make_shared<LOTAsset>();
    LOTAsset *asset = sharedAsset.get();
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "ty")) {    /* Type of layer: Shape. Value 4.*/
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            asset->mAssetType = GetInt();
        } else if (0 == strcmp(key, "id")) {    /* reference id*/
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            asset->mRefId = std::string(GetString());
        }else if (0 == strcmp(key, "layers")) {
            RAPIDJSON_ASSERT(PeekType() == kArrayType);
            EnterArray();
            while (NextArrayValue()) {
                std::shared_ptr<LOTData> layer = parseLayer();
                asset->mLayers.push_back(layer);
            }
        } else {
    #ifdef DEBUG_PARSER
            vWarning<<"Asset Attribute Skipped : "<<key;
    #endif
            Skip(key);
        }
    }
    return sharedAsset;
}

void LottieParserImpl::parseLayers(LOTCompositionData *composition)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        std::shared_ptr<LOTData> layer = parseLayer();
        composition->mChildren.push_back(layer);
    }
}

LottieColor
LottieParserImpl::toColor(const char *str)
{
   LottieColor color;

   RAPIDJSON_ASSERT(strlen(str) == 7);
   RAPIDJSON_ASSERT(str[0] == '#');

   char tmp[3] = { '\0', '\0', '\0' };
   tmp[0] = str[1]; tmp[1] = str[2];
   color.r = std::strtol(tmp, NULL, 16)/ 255.0;

   tmp[0] = str[3]; tmp[1] = str[4];
   color.g = std::strtol(tmp, NULL, 16)/ 255.0;

   tmp[0] = str[5]; tmp[1] = str[6];
   color.b = std::strtol(tmp, NULL, 16)/ 255.0;

   return color;
}

MatteType
LottieParserImpl::getMatteType()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return MatteType::Alpha;
        break;
    case 2:
        return MatteType::AlphaInv;
        break;
    case 3:
        return MatteType::Luma;
        break;
    case 4:
        return MatteType::LumaInv;
        break;
    default:
        return MatteType::None;
        break;
    }
}

LayerType LottieParserImpl::getLayerType()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 0:
        return LayerType::Precomp;
        break;
    case 1:
        return LayerType::Solid;
        break;
    case 2:
        return LayerType::Image;
        break;
    case 3:
        return LayerType::Null;
        break;
    case 4:
        return LayerType::Shape;
        break;
    case 5:
        return LayerType::Text;
        break;
    default:
        return LayerType::Null;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/shape.json
 *
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseLayer()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    std::shared_ptr<LOTLayerData> sharedLayer = std::make_shared<LOTLayerData>();
    LOTLayerData *layer = sharedLayer.get();
    curLayerRef = layer;
    bool hasLayerRef = false;
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "ty")) {    /* Type of layer*/
            layer->mLayerType = getLayerType();
        } else if (0 == strcmp(key, "ind")) { /*Layer index in AE. Used for parenting and expressions.*/
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mId = GetInt();
        } else if (0 == strcmp(key, "parent")) { /*Layer Parent. Uses "ind" of parent.*/
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mParentId = GetInt();
        } else if (0 == strcmp(key, "refId")) { /*preComp Layer reference id*/
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            layer->mPreCompRefId = std::string(GetString());
            mLayersToUpdate.push_back(sharedLayer);
            hasLayerRef = true;
        }else if (0 == strcmp(key, "sr")) { // "Layer Time Stretching"
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mTimeStreatch = GetDouble();
        } else if (0 == strcmp(key, "tm")) { // time remapping
            parseProperty(layer->mTimeRemap);
        }else if (0 == strcmp(key, "ip")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mInFrame = std::round(GetDouble());
        } else if (0 == strcmp(key, "op")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mOutFrame = std::round(GetDouble());
        }  else if (0 == strcmp(key, "st")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mStartFrame = GetDouble();
        } else if (0 == strcmp(key, "bounds")) {
            layer->mBound = getRect();
        } else if (0 == strcmp(key, "bm")) {
            layer->mBlendMode = getBlendMode();
        } else if (0 == strcmp(key, "ks")) {
            RAPIDJSON_ASSERT(PeekType() == kObjectType);
            EnterObject();
            layer->mTransform = parseTransformObject();
        } else if (0 == strcmp(key, "shapes")) {
            parseShapesAttr(layer);
        } else if (0 == strcmp(key, "sw")) {
            layer->mSolidLayer.mWidth = GetInt();
        } else if (0 == strcmp(key, "sh")) {
            layer->mSolidLayer.mHeight = GetInt();
        } else if (0 == strcmp(key, "sc")) {
            layer->mSolidLayer.mColor = toColor(GetString());
        } else if (0 == strcmp(key, "tt")) {
            layer->mMatteType = getMatteType();
        } else if (0 == strcmp(key, "tt")) {
            layer->mMatteType = getMatteType();
        } else if (0 == strcmp(key, "hasMask")) {
            layer->mHasMask = GetBool();
        } else if (0 == strcmp(key, "masksProperties")) {
            parseMaskProperty(layer);
        }else {
    #ifdef DEBUG_PARSER
            vWarning<<"Layer Attribute Skipped : "<<key;
    #endif
            Skip(key);
        }
    }
    // update the static property of layer
    bool staticFlag = true;
    for (auto child : layer->mChildren) {
        staticFlag &= child.get()->isStatic();
    }

    for (auto mask : layer->mMasks) {
        staticFlag &= mask->isStatic();
    }

    layer->setStatic(staticFlag &&
                     layer->mTransform->isStatic() &&
                     !hasLayerRef);

    return sharedLayer;
}

void LottieParserImpl::parseMaskProperty(LOTLayerData *layer)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        layer->mMasks.push_back(parseMaskObject());
    }
}

std::shared_ptr<LOTMaskData>
LottieParserImpl::parseMaskObject()
{
    std::shared_ptr<LOTMaskData> sharedMask = std::make_shared<LOTMaskData>();
    LOTMaskData *obj = sharedMask.get();

    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "inv")) {
            obj->mInv = GetBool();
        } else if (0 == strcmp(key, "mode")) {
            const char *str = GetString();
            switch (str[0]) {
            case 'n':
                obj->mMode = LOTMaskData::Mode::None;
                break;
            case 'a':
                obj->mMode = LOTMaskData::Mode::Add;
                break;
            case 's':
                obj->mMode = LOTMaskData::Mode::Substarct;
                break;
            case 'i':
                obj->mMode = LOTMaskData::Mode::Intersect;
                break;
            default:
                obj->mMode = LOTMaskData::Mode::None;
                break;
            }
        } else if (0 == strcmp(key, "pt")) {
            parseShapeProperty(obj->mShape);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else {
            Skip(key);
        }
    }
    obj->mIsStatic = obj->mShape.isStatic() && obj->mOpacity.isStatic();
    return sharedMask;
}



void LottieParserImpl::parseShapesAttr(LOTLayerData *layer)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        parseObject(layer);
    }
}

std::shared_ptr<LOTData>
LottieParserImpl::parseObjectTypeAttr()
{
    RAPIDJSON_ASSERT(PeekType() == kStringType);
    const char *type = GetString();
    if (0 == strcmp(type, "gr")) {
        return parseGroupObject();
    } else if (0 == strcmp(type, "rc")) {
        return parseRectObject();
    } else if (0 == strcmp(type, "el")) {
        return parseEllipseObject();
    } else if (0 == strcmp(type, "tr")) {
        return parseTransformObject();
    } else if (0 == strcmp(type, "fl")) {
        return parseFillObject();
    } else if (0 == strcmp(type, "st")) {
        return parseStrokeObject();
    } else if (0 == strcmp(type, "gf")) {
        return parseGFillObject();
    } else if (0 == strcmp(type, "gs")) {
        return parseGStrokeObject();
    } else if (0 == strcmp(type, "sh")) {
        return parseShapeObject();
    } else if (0 == strcmp(type, "sr")) {
        return parsePolystarObject();
    } else if (0 == strcmp(type, "tm")) {
        return parseTrimObject();
    } else if (0 == strcmp(type, "rp")) {
        return parseReapeaterObject();
    } else {
#ifdef DEBUG_PARSER
        vDebug<<"The Object Type not yet handled = "<< type;
#endif
        return nullptr;
    }
}

void
LottieParserImpl::parseObject(LOTGroupData *parent)
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "ty")) {
            auto child = parseObjectTypeAttr();
            if (child) {
                if (child)
                parent->mChildren.push_back(child);
            }
        } else {
            Skip(key);
        }
    }
}

std::shared_ptr<LOTData>
LottieParserImpl::parseGroupObject()
{
    std::shared_ptr<LOTShapeGroupData> sharedGroup = std::make_shared<LOTShapeGroupData>();

    LOTShapeGroupData *group = sharedGroup.get();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "it")) {
            RAPIDJSON_ASSERT(PeekType() == kArrayType);
            EnterArray();
            while (NextArrayValue()) {
                RAPIDJSON_ASSERT(PeekType() == kObjectType);
                parseObject(group);
            }
            group->mTransform = std::dynamic_pointer_cast<LOTTransformData>(group->mChildren.back());
            group->mChildren.pop_back();
        } else {
            Skip(key);
        }
    }
    bool staticFlag = true;
    for (auto child : group->mChildren) {
        staticFlag &= child.get()->isStatic();
    }

    group->setStatic(staticFlag &&
                     group->mTransform->isStatic());

    return sharedGroup;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/rect.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseRectObject()
{
    std::shared_ptr<LOTRectData> sharedRect = std::make_shared<LOTRectData>();
    LOTRectData *obj = sharedRect.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPos);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mSize);
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj->mRound);
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else {
            Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() &&
                   obj->mSize.isStatic() &&
                   obj->mRound.isStatic());
    return sharedRect;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/ellipse.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseEllipseObject()
{
    std::shared_ptr<LOTEllipseData> sharedEllipse = std::make_shared<LOTEllipseData>();
    LOTEllipseData *obj = sharedEllipse.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPos);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mSize);
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else {
            Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() &&
                   obj->mSize.isStatic());
    return sharedEllipse;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/shape.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseShapeObject()
{
    std::shared_ptr<LOTShapeData> sharedShape = std::make_shared<LOTShapeData>();
    LOTShapeData *obj = sharedShape.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "ks")) {
            parseShapeProperty(obj->mShape);
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else {
#ifdef DEBUG_PARSER
            vDebug<<"Shape property ignored :"<<key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mShape.isStatic());

    return sharedShape;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/star.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parsePolystarObject()
{
    std::shared_ptr<LOTPolystarData> sharedPolystar = std::make_shared<LOTPolystarData>();
    LOTPolystarData *obj = sharedPolystar.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPos);
        } else if (0 == strcmp(key, "pt")) {
            parseProperty(obj->mPointCount);
        } else if (0 == strcmp(key, "ir")) {
            parseProperty(obj->mInnerRadius);
        } else if (0 == strcmp(key, "is")) {
            parseProperty(obj->mInnerRoundness);
        } else if (0 == strcmp(key, "or")) {
            parseProperty(obj->mOuterRadius);
        } else if (0 == strcmp(key, "os")) {
            parseProperty(obj->mOuterRoundness);
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj->mRotation);
        } else if (0 == strcmp(key, "sy")) {
            int starType = GetInt();
            if (starType == 1)
                obj->mType = LOTPolystarData::PolyType::Star;
            if (starType == 2)
                obj->mType = LOTPolystarData::PolyType::Polygon;
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else {
#ifdef DEBUG_PARSER
            vDebug<<"Polystar property ignored :"<<key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() &&
                   obj->mPointCount.isStatic() &&
                   obj->mInnerRadius.isStatic() &&
                   obj->mInnerRoundness.isStatic() &&
                   obj->mOuterRadius.isStatic() &&
                   obj->mOuterRoundness.isStatic() &&
                   obj->mRotation.isStatic());

    return sharedPolystar;
}

LOTTrimData::TrimType
LottieParserImpl::getTrimType()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return LOTTrimData::TrimType::Simultaneously;
        break;
    case 2:
        return LOTTrimData::TrimType::Individually;
        break;
    default:
        RAPIDJSON_ASSERT(0);
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/trim.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseTrimObject()
{
    std::shared_ptr<LOTTrimData> sharedTrim = std::make_shared<LOTTrimData>();
    LOTTrimData *obj = sharedTrim.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "s")) {
            parseProperty(obj->mStart);
        } else if (0 == strcmp(key, "e")) {
            parseProperty(obj->mEnd);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOffset);
        }  else if (0 == strcmp(key, "m")) {
            obj->mTrimType = getTrimType();
        } else {
#ifdef DEBUG_PARSER
            vDebug<<"Trim property ignored :"<<key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mStart.isStatic() &&
                   obj->mEnd.isStatic() &&
                   obj->mOffset.isStatic());
    curLayerRef->mHasPathOperator = true;
    return sharedTrim;
}

std::shared_ptr<LOTData>
LottieParserImpl::parseReapeaterObject()
{
    std::shared_ptr<LOTRepeaterData> sharedRepeater = std::make_shared<LOTRepeaterData>();
    LOTRepeaterData *obj = sharedRepeater.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "c")) {
            parseProperty(obj->mCopies);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOffset);
        } else if (0 == strcmp(key, "tr")) {
            obj->mTransform = parseTransformObject();
        } else {
#ifdef DEBUG_PARSER
            vDebug<<"Repeater property ignored :"<<key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mCopies.isStatic() &&
                   obj->mOffset.isStatic() &&
                   obj->mTransform->isStatic());

    return sharedRepeater;
}


/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/transform.json
 */
std::shared_ptr<LOTTransformData>
LottieParserImpl::parseTransformObject()
{
    std::shared_ptr<LOTTransformData> sharedTransform = std::make_shared<LOTTransformData>();
    LOTTransformData *obj = sharedTransform.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "a")) {
            parseProperty(obj->mAnchor);
        } else if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPosition);
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj->mRotation);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mScale);
        } else if (0 == strcmp(key, "sk")) {
            parseProperty(obj->mSkew);
        }  else if (0 == strcmp(key, "sa")) {
            parseProperty(obj->mSkewAxis);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else {
            Skip(key);
        }
    }
    obj->mStaticMatrix = obj->mAnchor.isStatic() &&
                         obj->mPosition.isStatic() &&
                         obj->mRotation.isStatic() &&
                         obj->mScale.isStatic() &&
                         obj->mSkew.isStatic() &&
                         obj->mSkewAxis.isStatic();
    obj->setStatic(obj->mStaticMatrix && obj->mOpacity.isStatic() );

    if (obj->mStaticMatrix)
        obj->cacheMatrix();

    return sharedTransform;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/fill.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseFillObject()
{
    std::shared_ptr<LOTFillData> sharedFill = std::make_shared<LOTFillData>();
    LOTFillData *obj = sharedFill.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "c")) {
            parseProperty(obj->mColor);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else if (0 == strcmp(key, "fillEnabled")) {
            obj->mEnabled = GetBool();
        } else if (0 == strcmp(key, "r")) {
            obj->mFillRule = getFillRule();
        } else {
#ifdef DEBUG_PARSER
            vWarning<<"Fill property skipped = "<<key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mColor.isStatic() &&
                   obj->mOpacity.isStatic());

    return sharedFill;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/helpers/lineCap.json
 */
CapStyle LottieParserImpl::getLineCap()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return CapStyle::Flat;
        break;
    case 2:
        return CapStyle::Round;
        break;
    default:
        return CapStyle::Square;
        break;
    }
}

FillRule LottieParserImpl::getFillRule()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return FillRule::Winding;
        break;
    case 2:
        return FillRule::EvenOdd;
        break;
    default:
        return FillRule::Winding;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/helpers/lineJoin.json
 */
JoinStyle LottieParserImpl::getLineJoin()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return JoinStyle::Miter;
        break;
    case 2:
        return JoinStyle::Round;
        break;
    default:
        return JoinStyle::Bevel;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/stroke.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseStrokeObject()
{
    std::shared_ptr<LOTStrokeData> sharedStroke = std::make_shared<LOTStrokeData>();
    LOTStrokeData *obj = sharedStroke.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "c")) {
            parseProperty(obj->mColor);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else if (0 == strcmp(key, "w")) {
            parseProperty(obj->mWidth);
        } else if (0 == strcmp(key, "fillEnabled")) {
            obj->mEnabled = GetBool();
        } else if (0 == strcmp(key, "lc")) {
            obj->mCapStyle = getLineCap();
        } else if (0 == strcmp(key, "lj")) {
            obj->mJoinStyle = getLineJoin();
        } else if (0 == strcmp(key, "ml")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            obj->mMeterLimit = GetDouble();
        } else if (0 == strcmp(key, "d")) {
            parseDashProperty(obj->mDash);
        } else {
#ifdef DEBUG_PARSER
            vWarning<<"Stroke property skipped = "<<key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mColor.isStatic() &&
                   obj->mOpacity.isStatic() &&
                   obj->mWidth.isStatic() &&
                   obj->mDash.mStatic);
    return sharedStroke;
}

void
LottieParserImpl::parseGradientProperty(LOTGradient *obj, const char *key)
{
    if (0 == strcmp(key, "t")) {
        RAPIDJSON_ASSERT(PeekType() == kNumberType);
        obj->mGradientType = GetInt();
    } else if (0 == strcmp(key, "o")) {
        parseProperty(obj->mOpacity);
    } else if (0 == strcmp(key, "s")) {
        parseProperty(obj->mStartPoint);
    } else if (0 == strcmp(key, "e")) {
        parseProperty(obj->mEndPoint);
    } else if (0 == strcmp(key, "h")) {
        parseProperty(obj->mHighlightLength);
    } else if (0 == strcmp(key, "a")) {
        parseProperty(obj->mHighlightAngle);
    } else if (0 == strcmp(key, "g")) {
        EnterObject();
        while (const char* key = NextObjectKey()) {
            if (0 == strcmp(key, "k")) {
                parseProperty(obj->mGradient);
            } else if (0 == strcmp(key, "p")) {
                obj->mColorPoints = GetInt();
            }  else {
                Skip(nullptr);
            }
        }
    } else {
#ifdef DEBUG_PARSER
            vWarning<<"Gradient property skipped = "<<key;
#endif
            Skip(key);
    }
    obj->setStatic(obj->mOpacity.isStatic() &&
                   obj->mStartPoint.isStatic() &&
                   obj->mEndPoint.isStatic() &&
                   obj->mHighlightAngle.isStatic() &&
                   obj->mHighlightLength.isStatic() &&
                   obj->mGradient.isStatic());

}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/gfill.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseGFillObject()
{
    std::shared_ptr<LOTGFillData> sharedGFill = std::make_shared<LOTGFillData>();
    LOTGFillData *obj = sharedGFill.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "r")) {
            obj->mFillRule = getFillRule();
        } else {
            parseGradientProperty(obj, key);
        }
    }
    return sharedGFill;
}

void LottieParserImpl::parseDashProperty(LOTDashProperty &dash)
{
    dash.mDashCount = 0;
    dash.mStatic = true;
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        RAPIDJSON_ASSERT(PeekType() == kObjectType);
        EnterObject();
        while (const char* key = NextObjectKey()) {
            if (0 == strcmp(key, "v")) {
                parseProperty(dash.mDashArray[dash.mDashCount++]);
            } else {
                Skip(key);
            }
        }
    }

    // update the staic proprty
    for (int i = 0 ; i < dash.mDashCount ; i++) {
        if (!dash.mDashArray[i].isStatic()) {
            dash.mStatic = false;
            break;
        }
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/gstroke.json
 */
std::shared_ptr<LOTData>
LottieParserImpl::parseGStrokeObject()
{
    std::shared_ptr<LOTGStrokeData> sharedGStroke = std::make_shared<LOTGStrokeData>();
    LOTGStrokeData *obj = sharedGStroke.get();

    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "w")) {
            parseProperty(obj->mWidth);
        } else if (0 == strcmp(key, "lc")) {
            obj->mCapStyle = getLineCap();
        } else if (0 == strcmp(key, "lj")) {
            obj->mJoinStyle = getLineJoin();
        } else if (0 == strcmp(key, "ml")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            obj->mMeterLimit = GetDouble();
        } else if (0 == strcmp(key, "d")) {
            parseDashProperty(obj->mDash);
        }  else {
            parseGradientProperty(obj, key);
        }
    }

    obj->setStatic(obj->isStatic() &&
                   obj->mWidth.isStatic() &&
                   obj->mDash.mStatic);
    return sharedGStroke;
}

void LottieParserImpl::parseArrayValue(LottieColor &color)
{
    float val[4];
    int i=0;
    while (NextArrayValue()) {
        val[i++] = GetDouble();
    }

    color.r = val[0];
    color.g = val[1];
    color.b = val[2];
}

void LottieParserImpl::parseArrayValue(VPointF &pt)
{
    float val[4];
    int i=0;
    while (NextArrayValue()) {
        val[i++] = GetDouble();
    }
    pt.setX(val[0]);
    pt.setY(val[1]);
}

void LottieParserImpl::parseArrayValue(float &val)
{
    RAPIDJSON_ASSERT(0);
    val = GetDouble();
}

void LottieParserImpl::parseArrayValue(int &val)
{
    RAPIDJSON_ASSERT(0);
    val = GetInt();
}

void LottieParserImpl::parseArrayValue(std::vector<VPointF> &v)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        RAPIDJSON_ASSERT(PeekType() == kArrayType);
        EnterArray();
        VPointF pt;
        parseArrayValue(pt);
        v.push_back(pt);
    }
}

void LottieParserImpl::getValue(VPointF &pt)
{
    float val[4];
    int i=0;
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        val[i++] = GetDouble();
    }
    pt.setX(val[0]);
    pt.setY(val[1]);
}

void LottieParserImpl::getValue(float &val)
{
    if (PeekType() == kArrayType) {
        EnterArray();
        while (NextArrayValue()) {
            val = GetDouble();
        }
    } else if (PeekType() == kNumberType) {
        val = GetDouble();
    } else {
        RAPIDJSON_ASSERT(0);
    }
}

void LottieParserImpl::getValue(LottieColor &color)
{
    float val[4];
    int i=0;
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        val[i++] = GetDouble();
    }
    color.r = val[0];
    color.g = val[1];
    color.b = val[2];
}

void LottieParserImpl::parseArrayValue(LottieGradient &grad)
{
    while (NextArrayValue()) {
        grad.mGradient.push_back(GetDouble());
    }
}

void LottieParserImpl::getValue(LottieGradient &grad)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        grad.mGradient.push_back(GetDouble());
    }
}

void LottieParserImpl::getValue(int &val)
{
    if (PeekType() == kArrayType) {
        EnterArray();
        while (NextArrayValue()) {
            val = GetInt();
        }
    } else if (PeekType() == kNumberType) {
        val = GetInt();
    } else {
        RAPIDJSON_ASSERT(0);
    }
}

void LottieParserImpl::getValue(LottieShapeData &obj)
{
    std::vector<VPointF>    inPoint;          /* "i" */
    std::vector<VPointF>    outPoint;         /* "o" */
    std::vector<VPointF>    vertices;         /* "v" */
    std::vector<VPointF>    points;
    bool                     closed=false;

    /*
     * The shape object could be wrapped by a array
     * if its part of the keyframe object
     */
    bool arrayWrapper = (PeekType() == kArrayType);
    if (arrayWrapper)
         EnterArray();

    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "i")) {
            parseArrayValue(inPoint);
        } else if (0 == strcmp(key, "o")) {
            parseArrayValue(outPoint);
        } else if (0 == strcmp(key, "v")) {
            parseArrayValue(vertices);
        } else if (0 == strcmp(key, "c")) {
            closed = GetBool();
        }else {
            RAPIDJSON_ASSERT(0);
            Skip(nullptr);
        }
    }
    // exit properly from the array
    if (arrayWrapper)
        NextArrayValue();


/*
 * Convert the AE shape format to
 * list of bazier curves
 * The final structure will be Move +size*Cubic + Cubic (if the path is closed one)
 */
    if (inPoint.size() != outPoint.size() ||
        inPoint.size() != vertices.size()) {
        vCritical<<"The Shape data are corrupted";
        points = std::vector<VPointF>();
    } else {
        int size = vertices.size();
        points.reserve(3*size + 4);
        points.push_back(vertices[0]);
        for (int i =1; i <size ; i++ ) {
            points.push_back(vertices[i-1] + outPoint[i-1]); // CP1 = start + outTangent
            points.push_back(vertices[i] + inPoint[i]); // CP2 = end + inTangent
            points.push_back(vertices[i]); //end point
        }

        if (closed) {
            points.push_back(vertices[size-1] + outPoint[size-1]); // CP1 = start + outTangent
            points.push_back(vertices[0] + inPoint[0]); // CP2 = end + inTangent
            points.push_back(vertices[0]); //end point
        }
    }
    obj.mPoints = std::move(points);
    obj.mClosed = closed;
}

VPointF
LottieParserImpl::parseInperpolatorPoint()
{
    VPointF cp;
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while(const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "x")) {
            if (PeekType() == kNumberType) {
                cp.setX(GetDouble());
            } else {
                RAPIDJSON_ASSERT(PeekType() == kArrayType);
                EnterArray();
                while (NextArrayValue()) {
                    cp.setX(GetDouble());
                }
            }
        }
        if (0 == strcmp(key, "y")) {
            if (PeekType() == kNumberType) {
                cp.setY(GetDouble());
            } else {
                RAPIDJSON_ASSERT(PeekType() == kArrayType);
                EnterArray();
                while (NextArrayValue()) {
                    cp.setY(GetDouble());
                }
            }
        }
    }
    return cp;
}

template<typename T>
bool LottieParserImpl::parseKeyFrameValue(const char* key, LOTKeyFrameValue<T> &value)
{
    if (0 == strcmp(key, "s")) {
        getValue(value.mStartValue);
    } else if (0 == strcmp(key, "e")) {
        getValue(value.mEndValue);
    } else {
        return false;
    }
    return true;
}

template<>
bool LottieParserImpl::parseKeyFrameValue(const char* key, LOTKeyFrameValue<VPointF> &value)
{
    if (0 == strcmp(key, "s")) {
        getValue(value.mStartValue);
    } else if (0 == strcmp(key, "e")) {
        getValue(value.mEndValue);
    } else if (0 == strcmp(key, "ti")) {
        value.mPathKeyFrame = true;
        getValue(value.mInTangent);
    } else if (0 == strcmp(key, "to")) {
        value.mPathKeyFrame = true;
        getValue(value.mOutTangent);
    } else {
        return false;
    }
    return true;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/properties/multiDimensionalKeyframed.json
 */
template<typename T>
void LottieParserImpl::parseKeyFrame(LOTAnimInfo<T> &obj)
{
    EnterObject();
    LOTKeyFrame<T> keyframe;
    VPointF inTangent;
    VPointF outTangent;
    const char *interpolatorKey = nullptr;
    bool hold = false;
     while (const char* key = NextObjectKey()) {
         if (0 == strcmp(key, "i")) {
             inTangent = parseInperpolatorPoint();
         } else if (0 == strcmp(key, "o")) {
             outTangent = parseInperpolatorPoint();
         } else if (0 == strcmp(key, "n")) {
             if (PeekType() == kStringType) {
                interpolatorKey = GetString();
             } else {
                RAPIDJSON_ASSERT(PeekType() == kArrayType);
                EnterArray();
                while (NextArrayValue()) {
                    RAPIDJSON_ASSERT(PeekType() == kStringType);
                    interpolatorKey = GetString();
                }
             }
             continue;
         } else if (0 == strcmp(key, "t")) {
             keyframe.mStartFrame = GetDouble();
         } else if (parseKeyFrameValue(key, keyframe.mValue)) {
             continue;
         } else if (0 == strcmp(key, "h")) {
             hold = GetInt();
             continue;
         } else {
#ifdef DEBUG_PARSER
             vDebug<<"key frame property skipped = "<<key;
#endif
             Skip(key);
         }
     }

     if (!obj.mKeyFrames.empty()) {
         // update the endFrame value of current keyframe
         obj.mKeyFrames.back().mEndFrame = keyframe.mStartFrame;
     }

     if (hold) {
         interpolatorKey = "hold_interpolator";
         inTangent = VPointF();
         outTangent = VPointF();
         keyframe.mValue.mEndValue = keyframe.mValue.mStartValue;
         keyframe.mEndFrame = keyframe.mStartFrame;
     }

     // Try to find the interpolator from cache
     if (interpolatorKey) {
         auto search = compRef->mInterpolatorCache.find(interpolatorKey);
         if (search != compRef->mInterpolatorCache.end()) {
             keyframe.mInterpolator = search->second;
         } else {
             keyframe.mInterpolator = std::make_shared<VInterpolator>(VInterpolator(inTangent, outTangent));
             compRef->mInterpolatorCache[interpolatorKey] = keyframe.mInterpolator;
         }
         obj.mKeyFrames.push_back(keyframe);
     }
}


/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/properties/shapeKeyframed.json
 */

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/properties/shape.json
 */
void
LottieParserImpl::parseShapeProperty(LOTAnimatable<LottieShapeData> &obj)
{
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "k")) {
            if (PeekType() == kArrayType) {
                EnterArray();
                while (NextArrayValue()) {
                    RAPIDJSON_ASSERT(PeekType() == kObjectType);
                    if (!obj.mAnimInfo)
                        obj.mAnimInfo = std::make_shared<LOTAnimInfo<LottieShapeData>>();
                    parseKeyFrame(*obj.mAnimInfo.get());
                }
            } else {
                getValue(obj.mValue);
            }
        } else {
#ifdef DEBUG_PARSER
            vDebug<<"shape property ignored = "<<key;
#endif
            Skip(nullptr);
        }
    }
}

/*
 * https://github.com/airbnb/lottie-web/tree/master/docs/json/properties
 */
template<typename T>
void LottieParserImpl::parseProperty(LOTAnimatable<T> &obj)
{
    EnterObject();
    while (const char* key = NextObjectKey()) {
        if (0 == strcmp(key, "k")) {
            if (PeekType() == kNumberType) {
                /*single value property with no animation*/
                getValue(obj.mValue);
            } else {
                RAPIDJSON_ASSERT(PeekType() == kArrayType);
                EnterArray();
                while (NextArrayValue()) {
                    /* property with keyframe info*/
                    if (PeekType() == kObjectType) {
                        if (!obj.mAnimInfo)
                            obj.mAnimInfo = std::make_shared<LOTAnimInfo<T>>();
                        parseKeyFrame(*obj.mAnimInfo.get());
                    } else {
                        /* Read before modifying.
                         * as there is no way of knowing if the
                         * value of the array is either array of numbers
                         * or array of object without entering the array
                         * thats why this hack is there
                         */
                        RAPIDJSON_ASSERT(PeekType() == kNumberType);
                        /*multi value property with no animation*/
                        parseArrayValue(obj.mValue);
                        /*break here as we already reached end of array*/
                        break;
                    }
                }
            }
        }  else if (0 == strcmp(key, "ix")){
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            obj.mPropertyIndex = GetInt();
        } else {
            Skip(key);
        }
    }
}

class LOTDataInspector : public LOTDataVisitor
{
public:
    void visit(LOTCompositionData *obj) {
        vDebug<<"[COMP_START:: static:"<<obj->isStatic()<<" v:"<<obj->mVersion<<" [{ stFm endFm fmRate } { "<<obj->mStartFrame<<" "<<obj->mEndFrame<<" }]\n";
    }
    void visit(LOTLayerData *obj) {
        vDebug<<"[LAYER_START:: type:"<<layerType(obj->mLayerType)<<" id:"<<obj->mId<<" Pid:"<<obj->mParentId
               <<" static:"<<obj->isStatic()<<"[{ stFm endFm stTm tmStrch } { "
               <<obj->mInFrame<<" "<<obj->mOutFrame<<" "<<obj->mStartFrame<<" "<<obj->mTimeStreatch <<" }]";
    }
    void visit(LOTTransformData *t) {
        vDebug<<"[TRANSFORM: static: "<<t->isStatic()<<" ]";
    }
    void visit(LOTShapeGroupData *o) {
        vDebug<<"[GROUP_START:: static:"<<o->isStatic()<<"]";
    }
    void visit(LOTShapeData *s) {
        vDebug<<"[SHAPE: static:"<<s->isStatic()<<"]";
    }
    void visit(LOTRectData *r) {
        vDebug<<"[RECT: static:"<<r->isStatic()<<"]";
    }
    void visit(LOTEllipseData *e) {
        vDebug<<"[ELLIPSE: static:"<<e->isStatic()<<"]";
    }
    void visit(LOTTrimData *t) {
        vDebug<<"[TRIM: static: "<<t->isStatic()<<" ]";
    }
    void visit(LOTRepeaterData *r) {
        vDebug<<"[REPEATER: static:"<<r->isStatic()<<"]";
    }
    void visit(LOTFillData *f) {
        vDebug<<"[FILL: static:"<<f->isStatic()<<"]";
    }
    void visit(LOTGFillData *f) {
        vDebug<<"[GFILL: static:"<<f->isStatic()<<" ty:"<<f->mGradientType<<" s:"<<f->mStartPoint.value(0)<<" e:"<<f->mEndPoint.value(0)<<"]";
    }
    void visit(LOTGStrokeData *f) {
        vDebug<<"[GSTROKE: static:"<<f->isStatic()<<"]";
    }
    void visit(LOTStrokeData *s) {
        vDebug<<"[STROKE: static:"<<s->isStatic()<<"]";
    }
    void visitChildren(LOTGroupData *obj) {
        for(auto child :obj->mChildren)
            child.get()->accept(this);
        switch (obj->type()) {
        case LOTData::Type::Layer:
        {
            LOTLayerData *layer = static_cast<LOTLayerData *>(obj);
            vDebug<<"[LAYER_END:: type:"<<layerType(layer->mLayerType).c_str()<<" id:"<<layer->mId<<"\n";
            break;
        }
        case LOTData::Type::ShapeGroup:
            vDebug<<"[GROUP_END]";
            break;
        case LOTData::Type::Composition:
            vDebug<<"[COMP End ]\n";
            break;
        case LOTData::Type::Repeater:
            vDebug<<"[REPEATER End ]";
            break;
        default:
            break;
        }
    }
    std::string layerType(LayerType type) {
        switch (type) {
        case LayerType::Precomp:
            return "Precomp";
            break;
        case LayerType::Null:
            return "Null";
            break;
        case LayerType::Shape:
            return "Shape";
            break;
        case LayerType::Solid:
            return "Solid";
            break;
        case LayerType::Image:
            return "Image";
            break;
        case LayerType::Text:
            return "Text";
            break;
        default:
            return "Unknow";
            break;
        }
    }
};

LottieParser::~LottieParser()
{
    delete d;
}

LottieParser::LottieParser(char* str): d(new LottieParserImpl(str))
{
    d->parseComposition();
}

std::shared_ptr<LOTModel> LottieParser::model()
{
    std::shared_ptr<LOTModel> model= std::make_shared<LOTModel>();
     model->mRoot = d->composition();
     model->mRoot->processPathOperatorObjects();
     model->mRoot->processRepeaterObjects();

#ifdef DEBUG_PARSER
     LOTDataInspector inspector;
     model->mRoot->accept(&inspector);
#endif

     return model;
}

RAPIDJSON_DIAG_POP
