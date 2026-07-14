<script setup>
import { computed, onMounted, ref } from 'vue'

const MAX_METERS = 30
const yTicks = [30, 20, 10, 0, -10, -20, -30]

const clamp = (value, min, max) => Math.min(Math.max(value, min), max)

const parseHeightFromUrl = () => {
  const params = new URLSearchParams(window.location.search)
  const rawHeight = Number(params.get('height'))

  if (!Number.isFinite(rawHeight)) {
    return 0
  }

  return clamp(rawHeight, -MAX_METERS, MAX_METERS)
}

const targetHeight = ref(0)
const animatedHeight = ref(0)
const labelVisible = ref(false)

const activeDirection = computed(() => {
  if (targetHeight.value === 0) {
    return 0
  }

  return targetHeight.value > 0 ? 1 : -1
})

const isUpperInactive = computed(() => activeDirection.value < 0)
const isLowerInactive = computed(() => activeDirection.value > 0)

const isTickInactive = (tick) => {
  if (activeDirection.value === 0 || tick === 0) {
    return false
  }

  return activeDirection.value > 0 ? tick < 0 : tick > 0
}

const tickTop = (value) => `${((MAX_METERS - value) / (MAX_METERS * 2)) * 100}%`

const upperFillPercent = computed(() => {
  return `${(Math.max(animatedHeight.value, 0) / MAX_METERS) * 100}%`
})

const lowerFillPercent = computed(() => {
  return `${(Math.max(-animatedHeight.value, 0) / MAX_METERS) * 100}%`
})

const readoutTop = computed(() => {
  const normalizedHeight = animatedHeight.value / MAX_METERS
  const edgeOffsetPx = animatedHeight.value > 0 ? -1 : animatedHeight.value < 0 ? 1 : 0
  const safeInsetPx = 28

  return `clamp(${safeInsetPx}px, calc(50% - (${normalizedHeight} * (var(--bar-height) / 2)) + ${edgeOffsetPx}px), calc(100dvh - ${safeInsetPx}px))`
})

const formattedHeight = computed(() => {
  const rounded = Math.round(targetHeight.value * 10) / 10
  return `${rounded}m`
})

onMounted(() => {
  targetHeight.value = parseHeightFromUrl()

  requestAnimationFrame(() => {
    animatedHeight.value = targetHeight.value

    setTimeout(() => {
      labelVisible.value = true
    }, 1600)
  })
})
</script>

<template>
  <div class="screen">
    <div class="y-scale" aria-label="y-axis scale from -30m to 30m">
      <div
        v-for="tick in yTicks"
        :key="tick"
        class="tick"
        :class="{ 'tick-inactive': isTickInactive(tick) }"
        :style="{ top: tickTop(tick) }"
      >
        <span class="tick-label">{{ tick }}m</span>
        <span class="tick-mark"></span>
      </div>
    </div>
    <div class="bar-upper" :class="{ 'bar-inactive': isUpperInactive }">
      <div class="fill fill-upper" :style="{ height: upperFillPercent }"></div>
    </div>
    <div class="bar-lower" :class="{ 'bar-inactive': isLowerInactive }">
      <div class="fill fill-lower" :style="{ height: lowerFillPercent }"></div>
    </div>
    <div class="height-readout" :class="{ visible: labelVisible }" :style="{ top: readoutTop }">
      {{ formattedHeight }}
    </div>
  </div>
</template>

<style scoped>
.screen {
  --bg-top: #f5f8fb;
  --bg-mid: #e9eef5;
  --bg-bot: #d9e2ec;
  --bar-light: #f7f9fc;
  --bar-mid: #e2e8f0;
  --bar-dark: #cbd5e1;
  --bar-border: #8a97a6;
  --fill-deep: #0d9488;
  --fill-mid: #14b8a6;
  --fill-light: #67e8f9;
  --ink-strong: #1f2937;
  --ink-soft: #475569;
  --bar-width: 150px;
  --bar-height: 90dvh;

  height: 100dvh;
  display: flex;
  justify-content: center;
  align-items: center;
  margin: 0;
  position: relative;
  font-family: 'Sora', 'Avenir Next', 'Trebuchet MS', sans-serif;
  background:
    radial-gradient(circle at 12% 14%, rgba(255, 255, 255, 0.75) 0%, rgba(255, 255, 255, 0) 36%),
    radial-gradient(circle at 84% 78%, rgba(103, 232, 249, 0.2) 0%, rgba(103, 232, 249, 0) 42%),
    linear-gradient(165deg, var(--bg-top) 0%, var(--bg-mid) 54%, var(--bg-bot) 100%);
}

.bar-upper,
.bar-lower {
  position: absolute;
  overflow: hidden;
  left: 50%;
  width: var(--bar-width);
  height: calc(var(--bar-height) / 2);
  background: linear-gradient(
    to right,
    var(--bar-light) 0%,
    #e5e5e5 12%,
    var(--bar-mid) 50%,
    #c6c6c6 78%,
    var(--bar-dark) 100%
  );
  box-shadow:
    inset 1px 0 0 rgba(255, 255, 255, 0.7),
    inset -1px 0 0 rgba(31, 41, 55, 0.14),
    0 10px 24px rgba(15, 23, 42, 0.18);
  transform: translateX(-50%);
  z-index: 1;
}

.y-scale {
  position: absolute;
  top: 50%;
  left: calc(50% - (var(--bar-width) / 2) - 70px);
  height: var(--bar-height);
  width: 70px;
  transform: translateY(-50%);
  z-index: 4;
}

.fill {
  position: absolute;
  left: 0;
  width: 100%;
  height: 0;
  background: linear-gradient(
    to right,
    var(--fill-deep) 0%,
    #0ea5a3 30%,
    var(--fill-mid) 54%,
    #2dd4bf 78%,
    var(--fill-light) 100%
  );
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.3),
    inset 0 -1px 0 rgba(6, 78, 59, 0.24);
  transition: height 1.8s cubic-bezier(0.2, 0.8, 0.2, 1);
}

.fill-upper {
  bottom: 0;
}

.fill-upper::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 8px;
  background: linear-gradient(to bottom, rgba(255, 255, 255, 0.35), rgba(255, 255, 255, 0));
}

.fill-lower {
  top: 0;
}

.fill-lower::before {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 8px;
  background: linear-gradient(to top, rgba(255, 255, 255, 0.35), rgba(255, 255, 255, 0));
}

.tick {
  position: absolute;
  right: 0;
  transform: translateY(-50%);
  display: flex;
  align-items: center;
  gap: 8px;
}

.tick-mark {
  width: 18px;
  height: 3px;
  background: var(--ink-strong);
  border-radius: 2px;
  transition: background-color 220ms ease, opacity 220ms ease;
}

.tick-label {
  font-size: 24px;
  font-weight: 600;
  letter-spacing: 0.01em;
  line-height: 1;
  color: var(--ink-soft);
  transition: color 220ms ease, opacity 220ms ease;
}

.tick-inactive .tick-label {
  color: #8a94a3;
  opacity: 0.45;
}

.tick-inactive .tick-mark {
  background: #7f8a99;
  opacity: 0.4;
}

.bar-upper {
  bottom: calc(50% - 1px);
  border: 1px solid var(--bar-border);
  border-bottom: 0;
  border-radius: 8px 8px 0 0;
  box-shadow:
    inset 0 2px 0 rgba(255, 255, 255, 0.7),
    inset 1px 0 0 rgba(255, 255, 255, 0.7),
    inset -1px 0 0 rgba(31, 41, 55, 0.14),
    0 10px 24px rgba(15, 23, 42, 0.18);
}

.bar-lower {
  top: calc(50% - 1px);
  border: 1px solid var(--bar-border);
  border-top: 0;
  border-radius: 0 0 8px 8px;
  box-shadow:
    inset 0 -2px 0 rgba(31, 41, 55, 0.18),
    inset 1px 0 0 rgba(255, 255, 255, 0.7),
    inset -1px 0 0 rgba(31, 41, 55, 0.14),
    0 10px 24px rgba(15, 23, 42, 0.18);
}

.bar-inactive {
  filter: grayscale(0.85) saturate(0.45);
  opacity: 0.42;
  transition: filter 260ms ease, opacity 260ms ease;
}

.height-readout {
  position: absolute;
  left: calc(50% + (var(--bar-width) / 2) + 22px);
  transform: translateY(-50%) translateX(-6px);
  padding: 8px 12px;
  border-radius: 999px;
  font-size: 20px;
  line-height: 1;
  color: #ffffff;
  font-weight: 700;
  letter-spacing: 0.01em;
  background: linear-gradient(135deg, #0b766e, #0f9f95);
  box-shadow:
    0 10px 22px rgba(6, 95, 70, 0.28),
    inset 0 1px 0 rgba(255, 255, 255, 0.28);
  opacity: 0;
  transition:
    top 1.8s cubic-bezier(0.2, 0.8, 0.2, 1),
    opacity 260ms ease,
    transform 260ms ease;
  z-index: 5;
}

.height-readout.visible {
  opacity: 1;
  transform: translateY(-50%) translateX(0);
}

</style>