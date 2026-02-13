// - 메시지 형식:
//   { id: string, type: 'prefetch', resources: [{ url, responseType: 'json'|'text' }], timeoutMs?: number }
// 응답 형식:
//   { id, type: 'prefetchResult', results: [{ url, ok, status, data?, error? }] }
type Resource = { url: string; responseType: 'json' | 'text' };

const controllers: Map<string, AbortController> = new Map();

self.addEventListener('message', async (ev: MessageEvent) => {
    const msg = ev.data || {};
    try {
        if (msg.type === 'prefetch') {
            const id: string = msg.id || String(Date.now());
            const resources: Resource[] = Array.isArray(msg.resources) ? msg.resources : [];
            const timeoutMs: number = typeof msg.timeoutMs === 'number' ? msg.timeoutMs : 3000;

            const ac = new AbortController();
            controllers.set(id, ac);

            const timeout = setTimeout(() => {
                try {
                    ac.abort();
                } catch (e) {
                    // 무시
                }
            }, timeoutMs);

            const results = await Promise.all(resources.map(async (r) => {
                const out: any = {url: r.url, ok: false, status: 0};

                try {
                    const res = await fetch(r.url, {method: 'GET', signal: ac.signal, credentials: 'include'});
                    out.status = res.status || 0;

                    if (!res.ok) {
                        out.ok = false;
                        out.error = `HTTP ${res.status}`;
                        return out;
                    }

                    if (r.responseType === 'json') {
                        // JSON 파싱은 워커 내부에서 수행
                        const j = await res.json();
                        out.ok = true;
                        out.data = j;
                    } else {
                        const txt = await res.text();
                        out.ok = true;
                        out.data = txt;
                    }
                } catch (err: any) {
                    out.ok = false;
                    out.error = err && err.message ? err.message : String(err);
                }

                return out;
            }));

            clearTimeout(timeout);
            controllers.delete(id);

            (self as any).postMessage({id, type: 'prefetchResult', results});
        } else if (msg.type === 'abort') {
            const id = msg.id;

            const ac = controllers.get(id);
            if (ac) {
                ac.abort();
                controllers.delete(id);
            }

            (self as any).postMessage({id, type: 'aborted'});
        }
    } catch (e: any) {
        const id = msg && msg.id ? msg.id : String(Date.now());
        (self as any).postMessage({id, type: 'error', error: e && e.message ? e.message : String(e)});
    }
});
