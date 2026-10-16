#ifndef TINYTV_RUNTIME_METRICS_H
#define TINYTV_RUNTIME_METRICS_H
struct PerfCounters {
  uint32_t frames=0, decodeMaxUs=0, frameMaxUs=0, lastFrameUs=0;
  uint64_t decodeUs=0;
  uint32_t netOk[4]={}, netFail[4]={}, canceled=0;
  uint32_t loopMaxUs=0, pageMaxUs=0, firstMaxUs=0;
} perf;

void Perf_frame(uint32_t started);
void Perf_report();
#endif
