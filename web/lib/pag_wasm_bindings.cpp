/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <emscripten/bind.h>
#include <memory>
#include "banner/Banner.h"
#include "core/FontMetrics.h"
#include "core/ImageInfo.h"
#include "core/PathTypes.h"
#include "gpu/opengl/GLDefines.h"
#include "pag/MogicUtils.h"
#include "pag/pag.h"
#include "pag/types.h"
#include "platform/web/GPUDrawable.h"
#include "platform/web/NativeImage.h"
#include "postProcess/CropScaleProc.h"
#include "rendering/editing/StillImage.h"

using namespace emscripten;
using namespace pag;
using namespace mogic;

EMSCRIPTEN_BINDINGS(pag) {
  class_<PAGLayer>("_PAGLayer")
      .smart_ptr<std::shared_ptr<PAGLayer>>("_PAGLayer")
      .function("_uniqueID", optional_override([](PAGLayer &pagLayer) {
                  return static_cast<int>(pagLayer.uniqueID());
                }))
      .function("_uniqueIDCreatedByAE", optional_override([](PAGLayer &pagLayer) {
                  return static_cast<int>(pagLayer.uniqueIDCreatedByAE());
                }))
      .function("_layerType", optional_override([](PAGLayer &pagLayer) {
                  return static_cast<LayerType>(pagLayer.layerType());
                }))
      .function("_layerName", &PAGLayer::layerName)
      .function("_width", &PAGLayer::width)
      .function("_height", &PAGLayer::height)
      .function("_widthAfterTrans", &PAGLayer::widthAfterTrans)
      .function("_heightAfterTrans", &PAGLayer::heightAfterTrans)
      .function("_scale", &PAGLayer::scale)
      .function("_setScale", &PAGLayer::setScale)
      .function("_rotate", &PAGLayer::rotate)
      .function("_setRotate", &PAGLayer::setRotate)
      .function("_translate", &PAGLayer::translate)
      .function("_setTranslate", &PAGLayer::setTranslate)
      .function("_getBoundAfterTransform", &PAGLayer::getBoundAfterTransform)
      .function("_alpha", &PAGLayer::alpha)
      .function("_setAlpha", &PAGLayer::setAlpha)
      .function("_visible", &PAGLayer::visible)
      .function("_setVisible", &PAGLayer::setVisible)
      .function("_editableIndex", &PAGLayer::editableIndex)
      .function("_frameRate", &PAGLayer::frameRate)
      .function("_duration", optional_override([](PAGLayer &pagLayer) {
                  return static_cast<int>(pagLayer.duration());
                }))
      .function("_startTime", optional_override([](PAGLayer &pagLayer) {
                  return static_cast<int>(pagLayer.startTime());
                }))
      .function("_localTimeToGlobal", optional_override([](PAGLayer &pagLayer, int localTime) {
                  return static_cast<int>(pagLayer.localTimeToGlobal(localTime));
                }))
      .function("_globalToLocalTime", optional_override([](PAGLayer &pagLayer, int globalTime) {
                  return static_cast<int>(pagLayer.globalToLocalTime(globalTime));
                }));
  class_<PAGSolidLayer, base<PAGLayer>>("_PAGSolidLayer")
      .smart_ptr<std::shared_ptr<PAGSolidLayer>>("_PAGSolidLayer");
  class_<PAGTextLayer, base<PAGLayer>>("_PAGTextLayer")
      .smart_ptr<std::shared_ptr<PAGTextLayer>>("_PAGTextLayer")
      .function("_getTextData", &PAGTextLayer::getTextData)
      .function("_fillColor", &PAGTextLayer::fillColor)
      .function("_setFillColor", &PAGTextLayer::setFillColor)
      .function("_fontSize", &PAGTextLayer::fontSize)
      .function("_setFontSize", &PAGTextLayer::setFontSize)
      .function("_strokeColor", &PAGTextLayer::strokeColor)
      .function("_setStrokeColor", &PAGTextLayer::setStrokeColor)
      .function("_text", &PAGTextLayer::text)
      .function("_setText", &PAGTextLayer::setText);
  class_<PAGShapeLayer, base<PAGLayer>>("_PAGShapeLayer")
      .smart_ptr<std::shared_ptr<PAGShapeLayer>>("_PAGShapeLayer");
  class_<PAGImageLayer, base<PAGLayer>>("_PAGImageLayer")
      .smart_ptr<std::shared_ptr<PAGImageLayer>>("_PAGImageLayer")
      .function("_getBackGroundGaussianBlurState", &PAGImageLayer::getBackGroundGaussianBlurState)
      .function("_setBackGroundGaussianBlurState", &PAGImageLayer::setBackGroundGaussianBlurState)
      .function("_contentDuration", &PAGImageLayer::contentDuration)
      .function("_replaceImage", &PAGImageLayer::replaceImage)
      .function("_resetImage",
                optional_override([](PAGImageLayer &ly) { ly.replaceImage(nullptr); }))
      .function("_layerTimeToContent", &PAGImageLayer::layerTimeToContent)
      .function("_contentTimeToLayer", &PAGImageLayer::contentTimeToLayer)
      .function("_getDefaultImageData", &PAGImageLayer::getDefaultImageData)
      .function("_getDefaultPAGImageW", optional_override([](PAGImageLayer &ly) {
                  unsigned int w = 0;
                  unsigned int h = 0;
                  ly.getDefaultPAGImageWH(w, h);
                  return w;
                }))
      .function("_getDefaultPAGImageH", optional_override([](PAGImageLayer &ly) {
                  unsigned int w = 0;
                  unsigned int h = 0;
                  ly.getDefaultPAGImageWH(w, h);
                  return h;
                }))
      .function("_getPAGImageW", optional_override([](PAGImageLayer &ly) {
                  unsigned int w = 0;
                  unsigned int h = 0;
                  ly.getPAGImageWH(w, h);
                  return w;
                }))
      .function("_getPAGImageH", optional_override([](PAGImageLayer &ly) {
                  unsigned int w = 0;
                  unsigned int h = 0;
                  ly.getPAGImageWH(w, h);
                  return h;
                }));

  class_<PAGComposition, base<PAGLayer>>("_PAGComposition")
      .smart_ptr<std::shared_ptr<PAGComposition>>("_PAGComposition")
      .function("_swapLayer", &PAGComposition::swapLayer)
      .function("_swapLayerAt", &PAGComposition::swapLayerAt)
      .function("_getLayerIndex", &PAGComposition::getLayerIndex)
      .function("_setLayerIndex", &PAGComposition::setLayerIndex)
      .function("_getLayerAt", &PAGComposition::getLayerAt)
      .function("_getSolidLayerAt", optional_override([](PAGComposition &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<PAGSolidLayer>(ly);
                }))
      .function("_getTextLayerAt", optional_override([](PAGComposition &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<PAGTextLayer>(ly);
                }))
      .function("_getShapeLayerAt", optional_override([](PAGComposition &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<PAGShapeLayer>(ly);
                }))
      .function("_getImageLayerAt", optional_override([](PAGComposition &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<PAGImageLayer>(ly);
                }))
      .function("_getPreComposeLayerAt", optional_override([](PAGComposition &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<PAGComposition>(ly);
                }))
      .function("_numChildren", &PAGComposition::numChildren);
  class_<PAGFile, base<PAGComposition>>("_PAGFile")
      .smart_ptr<std::shared_ptr<PAGFile>>("_PAGFile")
      .class_function("_Load", optional_override([](uintptr_t bytes, size_t length) {
                        return PAGFile::Load(reinterpret_cast<void *>(bytes), length);
                      }))
      .function("_numImages", &PAGFile::numImages)
      .function("_numVideos", &PAGFile::numVideos)
      .function("_numTexts", &PAGFile::numTexts)
      .function("_getTextData", &PAGFile::getTextData)
      .function("_replaceText", &PAGFile::replaceText)
      .function("_replaceImage", &PAGFile::replaceImage)
      .function("_getLayersByEditableIndex",
                optional_override([](PAGFile &pagFile, int editableIndex, LayerType layerType) {
                  return pagFile.getLayersByEditableIndex(editableIndex, layerType);
                }))
      .function("_timeStretchMode", &PAGFile::timeStretchMode)
      .function("_setTimeStretchMode", &PAGFile::setTimeStretchMode)
      .function("_setDuration", optional_override([](PAGFile &pagFile, int duration) {
                  return pagFile.setDuration(static_cast<int64_t>(duration));
                }));

  class_<PAGSurface>("_PAGSurface")
      .smart_ptr<std::shared_ptr<PAGSurface>>("_PAGSurface")
      .class_function("_FromCanvas", optional_override([](const std::string &canvasID) {
                        return PAGSurface::MakeFrom(GPUDrawable::FromCanvasID(canvasID));
                      }))
      .class_function("_FromTexture",
                      optional_override([](int textureID, int width, int height, bool flipY) {
                        GLTextureInfo glInfo = {};
                        glInfo.target = GL::TEXTURE_2D;
                        glInfo.id = static_cast<unsigned>(textureID);
                        glInfo.format = GL::RGBA8;
                        BackendTexture glTexture(glInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGSurface::MakeFrom(glTexture, origin);
                      }))
      .class_function("_FromFrameBuffer",
                      optional_override([](int frameBufferID, int width, int height, bool flipY) {
                        GLFrameBufferInfo glFrameBufferInfo = {};
                        glFrameBufferInfo.id = static_cast<unsigned>(frameBufferID);
                        glFrameBufferInfo.format = GL::RGBA8;
                        BackendRenderTarget glRenderTarget(glFrameBufferInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGSurface::MakeFrom(glRenderTarget, origin);
                      }))
      .function("_width", &PAGSurface::width)
      .function("_height", &PAGSurface::height)
      .function("_updateSize", &PAGSurface::updateSize);

  class_<PAGImage>("_PAGImage")
      .smart_ptr<std::shared_ptr<PAGImage>>("_PAGImage")
      .class_function("_FromBytes", optional_override([](uintptr_t bytes, size_t length) {
                        return PAGImage::FromBytes(reinterpret_cast<void *>(bytes), length);
                      }))
      .class_function("_FromNativeImage", optional_override([](val nativeImage) {
                        return std::static_pointer_cast<PAGImage>(
                            StillImage::FromImage(NativeImage::MakeFrom(nativeImage)));
                      }))
      .function("_width", &PAGImage::width)
      .function("_height", &PAGImage::height)
      .function("_scaleMode", &PAGImage::scaleMode)
      .function("_setScaleMode", &PAGImage::setScaleMode);

  class_<PAGPlayer>("_PAGPlayer")
      .smart_ptr_constructor("_PAGPlayer", &std::make_shared<PAGPlayer>)
      .function("_setProgress", &PAGPlayer::setProgress)
      .function("_flush", &PAGPlayer::flush)
      .function("_duration", optional_override([](PAGPlayer &pagPlayer) {
                  return static_cast<int>(pagPlayer.duration());
                }))
      .function("_getProgress", &PAGPlayer::getProgress)
      .function("_videoEnabled", &PAGPlayer::videoEnabled)
      .function("_setVideoEnabled", &PAGPlayer::setVideoEnabled)
      .function("_cacheEnabled", &PAGPlayer::cacheEnabled)
      .function("_setCacheEnabled", &PAGPlayer::setCacheEnabled)
      .function("_cacheScale", &PAGPlayer::cacheScale)
      .function("_setCacheScale", &PAGPlayer::setCacheScale)
      .function("_maxFrameRate", &PAGPlayer::maxFrameRate)
      .function("_setMaxFrameRate", &PAGPlayer::setMaxFrameRate)
      .function("_scaleMode", &PAGPlayer::scaleMode)
      .function("_setScaleMode", &PAGPlayer::setScaleMode)
      .function("_setSurface", &PAGPlayer::setSurface)
      .function("_getComposition", optional_override([](PAGPlayer &pagPlayer) {
                  return std::static_pointer_cast<PAGFile>(pagPlayer.getComposition());
                }))
      .function("_setComposition",
                optional_override([](PAGPlayer &pagPlayer, std::shared_ptr<PAGFile> &pagFile) {
                  pagPlayer.setComposition(std::move(pagFile));
                }));

  class_<ImageInfo>("ImageInfo")
      .property("width", &ImageInfo::width)
      .property("height", &ImageInfo::height)
      .property("rowBytes", &ImageInfo::rowBytes)
      .property("colorType", &ImageInfo::colorType);

  class_<Matrix>("Matrix")
      .property("a", &Matrix::getScaleX)
      .property("b", &Matrix::getSkewY)
      .property("c", &Matrix::getSkewX)
      .property("d", &Matrix::getScaleY)
      .property("tx", &Matrix::getTranslateX)
      .property("ty", &Matrix::getTranslateY);

  class_<TextDocument>("TextDocument")
      .smart_ptr<std::shared_ptr<TextDocument>>("TextDocument")
      .property("applyFill", &TextDocument::applyFill)
      .property("applyStroke", &TextDocument::applyStroke)
      .property("baselineShift", &TextDocument::baselineShift)
      .property("boxText", &TextDocument::boxText)
      .property("boxTextPos", &TextDocument::boxTextPos)
      .property("boxTextSize", &TextDocument::boxTextSize)
      .property("firstBaseLine", &TextDocument::firstBaseLine)
      .property("fauxBold", &TextDocument::fauxBold)
      .property("fauxItalic", &TextDocument::fauxItalic)
      .property("fillColor", &TextDocument::fillColor)
      .property("fontFamily", &TextDocument::fontFamily)
      .property("fontStyle", &TextDocument::fontStyle)
      .property("fontSize", &TextDocument::fontSize)
      .property("strokeColor", &TextDocument::strokeColor)
      .property("strokeOverFill", &TextDocument::strokeOverFill)
      .property("strokeWidth", &TextDocument::strokeWidth)
      .property("text", &TextDocument::text)
      .property("justification", &TextDocument::justification)
      .property("leading", &TextDocument::leading)
      .property("tracking", &TextDocument::tracking)
      .property("backgroundColor", &TextDocument::backgroundColor)
      .property("backgroundAlpha", &TextDocument::backgroundAlpha)
      .property("direction", &TextDocument::direction);

  class_<Stroke>("Stroke")
      .property("width", &Stroke::width)
      .property("cap", &Stroke::cap)
      .property("join", &Stroke::join)
      .property("miterLimit", &Stroke::miterLimit);

  value_object<FontMetrics>("FontMetrics")
      .field("ascent", &FontMetrics::ascent)
      .field("descent", &FontMetrics::descent)
      .field("xHeight", &FontMetrics::xHeight)
      .field("capHeight", &FontMetrics::capHeight);

  value_object<Rect>("Rect")
      .field("left", &Rect::left)
      .field("top", &Rect::top)
      .field("right", &Rect::right)
      .field("bottom", &Rect::bottom);

  value_object<Point>("Point").field("x", &Point::x).field("y", &Point::y);

  value_object<Color>("Color")
      .field("red", &Color::red)
      .field("green", &Color::green)
      .field("blue", &Color::blue);

  enum_<PathFillType>("PathFillType")
      .value("WINDING", PathFillType::Winding)
      .value("EVEN_ODD", PathFillType::EvenOdd)
      .value("INVERSE_WINDING", PathFillType::InverseWinding)
      .value("INVERSE_EVEN_ODD", PathFillType::InverseEvenOdd);

  enum_<ColorType>("ColorType")
      .value("Unknown", ColorType::Unknown)
      .value("ALPHA_8", ColorType::ALPHA_8)
      .value("RGBA_8888", ColorType::RGBA_8888)
      .value("BGRA_8888", ColorType::BGRA_8888);

  enum_<LayerType>("LayerType")
      .value("Unknown", LayerType::Unknown)
      .value("Null", LayerType::Null)
      .value("Solid", LayerType::Solid)
      .value("Text", LayerType::Text)
      .value("Shape", LayerType::Shape)
      .value("Image", LayerType::Image)
      .value("PreCompose", LayerType::PreCompose);

  enum_<Stroke::Cap>("Cap")
      .value("Miter", Stroke::Cap::Butt)
      .value("Round", Stroke::Cap::Round)
      .value("Bevel", Stroke::Cap::Square);

  enum_<Stroke::Join>("Join")
      .value("Miter", Stroke::Join::Miter)
      .value("Round", Stroke::Join::Round)
      .value("Bevel", Stroke::Join::Bevel);

  register_vector<std::shared_ptr<PAGLayer>>("VectorPAGLayer");
  register_vector<std::string>("VectorString");
  register_vector<Point>("VectorPoint");

  function("_SetFallbackFontNames", optional_override([](std::vector<std::string> fontNames) {
             PAGFont::SetFallbackFontNames(fontNames);
           }));

  value_object<CropProcParam>("CropProcParam")
      .field("leftCeilingX", &CropProcParam::leftCeilingX)
      .field("leftCeilingY", &CropProcParam::leftCeilingY)
      .field("rightFloorX", &CropProcParam::rightFloorX)
      .field("rightFloorY", &CropProcParam::rightFloorY)
      .field("segmentHeight", &CropProcParam::segmentHeight)
      .field("segmentWidth", &CropProcParam::segmentWidth)
      .field("zoomHeight", &CropProcParam::zoomHeight)
      .field("zoomWidth", &CropProcParam::zoomWidth);

  function("_BatchCropScaleProcess",
           optional_override([](std::shared_ptr<PAGSurface> &surface,
                                std::shared_ptr<PAGImage> &originImage, uintptr_t data,
                                CropProcParam param) -> std::shared_ptr<PAGImage> {
             return CropScaleProc::batchCropScaleProcess(surface, originImage,
                                                         reinterpret_cast<uint8_t *>(data), param);
           }));

  enum_<MogicClipType>("MogicClipType")
      .value("EdUnknown", MogicClipType::EdUnknown)
      .value("EdText", MogicClipType::EdText)
      .value("EdImage", MogicClipType::EdImage)
      .value("EdVideo", MogicClipType::EdVideo)
      .value("EdOral", MogicClipType::EdOral);

  function("_isEditableLayer",
           optional_override([](std::shared_ptr<PAGLayer> &pagLayer) -> MogicClipType {
             return static_cast<MogicClipType>(pag::MogicPagClip::isEditableLayer(pagLayer));
           }));

  // banner相关
  value_object<GLColor>("GLColor")
      .field("r", &GLColor::r)
      .field("g", &GLColor::g)
      .field("b", &GLColor::b)
      .field("a", &GLColor::a);

  value_object<TextProp>("TextProp")
      .field("fontName", &TextProp::fontName)
      .field("fontSize", &TextProp::fontSize)
      .field("fillColor", &TextProp::fillColor)
      .field("strokeColor", &TextProp::strokeColor)
      .field("leading", &TextProp::leading)
      .field("tracking", &TextProp::tracking)
      .field("verticalScale", &TextProp::verticalScale)
      .field("horizonScale", &TextProp::horizonScale)
      .field("bold", &TextProp::bold)
      .field("italic", &TextProp::italic)
      .field("fontCaps", &TextProp::fontCaps)
      .field("underLine", &TextProp::underLine)
      .field("strikethrough", &TextProp::strikethrough)
      .field("justification", &TextProp::justification)
      .field("strokeSize", &TextProp::strokeSize);

  class_<BannerLayer>("_BannerLayer")
      .smart_ptr<std::shared_ptr<BannerLayer>>("_BannerLayer")
      .function("_getLayerType", &BannerLayer::getLayerType)
      .function("_getLayerName", &BannerLayer::getLayerName)
      .function("_updateLayerBoundBox", &BannerLayer::updateLayerBoundBox)
      .function("_getLayerBoundBox", optional_override([](BannerLayer &ly) {
                  auto boundBox = ly.getLayerBoundBox();
                  Rect jsBoundbox = {boundBox.left, ly.getRootNode().getPSDSize().y - boundBox.top,
                                     boundBox.right,
                                     ly.getRootNode().getPSDSize().y - boundBox.bottom};
                  return jsBoundbox;
                }));

  class_<BannerImageLayer, base<BannerLayer>>("_BannerImageLayer")
      .smart_ptr<std::shared_ptr<BannerImageLayer>>("_BannerImageLayer")
      .function("_getLayerImagePath", &BannerImageLayer::getLayerImagePath)
      .function("_updateLayerImagePath", &BannerImageLayer::updateLayerImagePath)
      .function("_updateLayerImage",
                optional_override([](BannerImageLayer &imgLayer, uintptr_t imageData,
                                     const int &wid, const int &hei, const int &nChannle) {
                  return imgLayer.updateLayerImage(reinterpret_cast<uint8_t *>(imageData), wid, hei,
                                                   nChannle);
                }));

  register_vector<unsigned int>("VectorUint");
  register_vector<float>("VectorFloat");
  register_vector<GLColor>("VectorGLColor");
  register_vector<TextProp>("VectorTextProp");

  class_<BannerTextLayer, base<BannerLayer>>("_BannerTextLayer")
      .smart_ptr<std::shared_ptr<BannerTextLayer>>("_BannerTextLayer")
      .function("_getLengthArray", &BannerTextLayer::getLengthArray)
      .function("_updateLengthArray", &BannerTextLayer::updateLengthArray)
      .function("_isHasStroke", &BannerTextLayer::isHasStroke)
      .function("_getStrokeSize", &BannerTextLayer::getStrokeSize)
      .function("_updateText", &BannerTextLayer::updateText)
      .function("_updateHasStroke", &BannerTextLayer::updateHasStroke)
      .function("_updateStrokeSize", &BannerTextLayer::updateStrokeSize)
      .function("_getTextProps", &BannerTextLayer::getTextProps)
      .function("_updateTextProps", &BannerTextLayer::updateTextProps);

  class_<BannerCompositionLayer, base<BannerLayer>>("_BannerCompositionLayer")
      .smart_ptr<std::shared_ptr<BannerCompositionLayer>>("_BannerCompositionLayer")
      // 获取图层数量
      .function("_numberChildren", &BannerCompositionLayer::numberChildren)
      .function("_getChildren", &BannerCompositionLayer::getChildren)
      // 添加图层
      .function("_addLayer", &BannerCompositionLayer::addLayer)
      .function("_addLayerAt", &BannerCompositionLayer::addLayerAt)
      // 删除图层
      .function("_removeLayer", &BannerCompositionLayer::removeLayer)
      .function("_removeLayerAt", &BannerCompositionLayer::removeLayerAt)
      // 获取图层接口
      .function("_getLayerAt", &BannerCompositionLayer::getLayerAt)
      .function("_getBannerLayerAt", optional_override([](BannerCompositionLayer &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<BannerLayer>(ly);
                }))
      .function("_getBannerTextLayerAt",
                optional_override([](BannerCompositionLayer &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<BannerTextLayer>(ly);
                }))
      .function("_getBannerImageLayerAt",
                optional_override([](BannerCompositionLayer &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<BannerImageLayer>(ly);
                }))
      .function("_getBannerCompositionLayerAt",
                optional_override([](BannerCompositionLayer &com, int Index) {
                  auto ly = com.getLayerAt(Index);
                  return std::dynamic_pointer_cast<BannerCompositionLayer>(ly);
                }));

  class_<BannerRootNode, base<BannerCompositionLayer>>("_BannerRootNode")
      .smart_ptr<std::shared_ptr<BannerRootNode>>("_BannerRootNode")
      .class_function("_createRoot", optional_override([](const std::string &jsonStr,
                                                          std::shared_ptr<PAGSurface> surface) {
                        auto jsonTemplate = nlohmann::json::parse(jsonStr, nullptr, false);
                        return BannerRootNode::createRoot(jsonTemplate.at(0), surface);
                      }))
      .function("_draw", &BannerRootNode::draw)
      .function("_setPSDSize", &BannerRootNode::setPSDSize)
      .function("_getPSDSize", &BannerRootNode::getPSDSize)
      .function("_setScreenSize", &BannerRootNode::setScreenSize)
      .function("_getScreenSize", &BannerRootNode::getScreenSize)
      .function("_setCleanColor", &BannerRootNode::setCleanColor)
      .function("_getCleanColor", &BannerRootNode::getCleanColor)
      .function("_getPAGImage", &BannerRootNode::getPAGImage);

  // banner 字体相关
  function("_isFontRegistered", optional_override([](const std::string &fontName) {
             return BannerFontUtils::getInstance().isFontRegistered(fontName);
           }));

  function("_registerFont", optional_override([](uintptr_t fontData, const uint32_t &len,
                                                 const std::string &fontName) {
             return BannerFontUtils::getInstance().registerFont(
                 reinterpret_cast<uint8_t *>(fontData), len, fontName);
           }));

  function("_unregisterFont", optional_override([](const std::string &fontName) {
             BannerFontUtils::getInstance().unregisterFont(fontName);
           }));

  function("_registerFallbackFont", optional_override([](uintptr_t fontData, const uint32_t &len,
                                                         const std::string &fontName) {
             return BannerFontUtils::getInstance().registerFallbackFont(
                 reinterpret_cast<uint8_t *>(fontData), len, fontName);
           }));
}
