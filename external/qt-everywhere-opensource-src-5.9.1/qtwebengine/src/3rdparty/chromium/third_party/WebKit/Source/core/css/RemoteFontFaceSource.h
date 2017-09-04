// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFontFaceSource_h
#define RemoteFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include "core/fetch/FontResource.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSFontSelector;

enum FontDisplay {
  FontDisplayAuto,
  FontDisplayBlock,
  FontDisplaySwap,
  FontDisplayFallback,
  FontDisplayOptional,
  FontDisplayEnumMax
};

class RemoteFontFaceSource final : public CSSFontFaceSource,
                                   public FontResourceClient {
  USING_PRE_FINALIZER(RemoteFontFaceSource, dispose);
  USING_GARBAGE_COLLECTED_MIXIN(RemoteFontFaceSource);

 public:
  enum DisplayPeriod { BlockPeriod, SwapPeriod, FailurePeriod };

  explicit RemoteFontFaceSource(FontResource*, CSSFontSelector*, FontDisplay);
  ~RemoteFontFaceSource() override;
  void dispose();

  bool isLoading() const override;
  bool isLoaded() const override;
  bool isValid() const override;
  DisplayPeriod getDisplayPeriod() const { return m_period; }

  void beginLoadIfNeeded() override;

  void notifyFinished(Resource*) override;
  void fontLoadShortLimitExceeded(FontResource*) override;
  void fontLoadLongLimitExceeded(FontResource*) override;
  String debugName() const override { return "RemoteFontFaceSource"; }

  bool isBlank() override { return m_period == BlockPeriod; }

  // For UMA reporting
  bool hadBlankText() override { return m_histograms.hadBlankText(); }
  void paintRequested() { m_histograms.fallbackFontPainted(m_period); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  PassRefPtr<SimpleFontData> createFontData(const FontDescription&) override;
  PassRefPtr<SimpleFontData> createLoadingFallbackFontData(
      const FontDescription&);
  void pruneTable();

 private:
  class FontLoadHistograms {
    DISALLOW_NEW();

   public:
    // Should not change following order in CacheHitMetrics to be used for
    // metrics values.
    enum CacheHitMetrics { Miss, DiskHit, DataUrl, MemoryHit, CacheHitEnumMax };
    enum DataSource {
      FromUnknown,
      FromDataURL,
      FromMemoryCache,
      FromDiskCache,
      FromNetwork
    };

    FontLoadHistograms(DataSource dataSource, FontDisplay fontDisplay)
        : m_loadStartTime(0),
          m_blankPaintTime(0),
          m_isLongLimitExceeded(false),
          m_dataSource(dataSource),
          m_fontDisplay(fontDisplay) {}
    void loadStarted();
    void fallbackFontPainted(DisplayPeriod);
    void fontLoaded(bool isCorsFailed,
                    bool loadError,
                    bool isInterventionTriggered);
    void longLimitExceeded(bool isInterventionTriggered);
    void recordFallbackTime(const FontResource*);
    void recordRemoteFont(const FontResource*, bool isInterventionTriggered);
    bool hadBlankText() { return m_blankPaintTime; }
    DataSource dataSource() { return m_dataSource; }
    void maySetDataSource(DataSource);

   private:
    void recordLoadTimeHistogram(const FontResource*,
                                 int duration,
                                 bool isInterventionTriggered);
    void recordInterventionResult(bool isTriggered);
    CacheHitMetrics dataSourceMetricsValue();
    double m_loadStartTime;
    double m_blankPaintTime;
    bool m_isLongLimitExceeded;
    DataSource m_dataSource;
    FontDisplay m_fontDisplay;
  };

  void switchToSwapPeriod();
  void switchToFailurePeriod();
  bool shouldTriggerWebFontsIntervention();
  bool isLowPriorityLoadingAllowedForRemoteFont() const override;

  Member<FontResource> m_font;
  Member<CSSFontSelector> m_fontSelector;
  const FontDisplay m_display;
  DisplayPeriod m_period;
  FontLoadHistograms m_histograms;
  bool m_isInterventionTriggered;
};

}  // namespace blink

#endif
