// Thin dark rain over the hero, like the game's. Pauses off-screen; off for reduced motion.
(() => {
  const canvas = document.querySelector('.rain');
  if (!canvas || matchMedia('(prefers-reduced-motion: reduce)').matches) return;
  const ctx = canvas.getContext('2d');
  let w = 0, h = 0, drops = [], running = true, last = performance.now();

  function resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    w = canvas.clientWidth; h = canvas.clientHeight;
    canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const count = Math.round(170 * Math.min(1.6, (w * h) / (1280 * 720)));
    drops = Array.from({ length: count }, () => ({
      x: Math.random() * w * 1.3 - w * 0.15,
      y: Math.random() * h,
      len: h * (0.028 + Math.random() * 0.022),
      speed: h * (1.6 + Math.random() * 0.7),
      a: 0.16 + Math.random() * 0.2,
    }));
  }

  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    ctx.clearRect(0, 0, w, h);
    ctx.lineWidth = Math.max(1, h / 700);
    for (const d of drops) {
      d.y += d.speed * dt;
      d.x += d.speed * dt * 0.28;
      if (d.y > h + d.len) { d.y = -d.len; d.x = Math.random() * w * 1.3 - w * 0.3; }
      ctx.strokeStyle = `rgba(30,31,28,${d.a})`;
      ctx.beginPath();
      ctx.moveTo(d.x, d.y);
      ctx.lineTo(d.x + d.len * 0.28, d.y + d.len);
      ctx.stroke();
    }
    if (running) requestAnimationFrame(frame);
  }

  new IntersectionObserver(([e]) => {
    const was = running;
    running = e.isIntersecting;
    if (running && !was) { last = performance.now(); requestAnimationFrame(frame); }
  }).observe(canvas);

  addEventListener('resize', resize);
  resize();
  requestAnimationFrame(frame);
})();
