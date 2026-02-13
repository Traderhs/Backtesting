type Resource = { url: string; responseType: 'json' | 'text' };

let worker: Worker | null = null;
let nextId = 1;
const pending = new Map<string, { resolve: Function; reject: Function; timer?: number }>();

function createWorker(): Worker | null {
    if (worker) {
        return worker;
    }

    try {
        worker = new Worker(new URL('../Workers/ResultsWorker.ts', import.meta.url), {type: 'module'});

        worker.onmessage = (ev: MessageEvent) => {
            const msg = ev.data || {};
            const id = String(msg.id || '');

            const entry = pending.get(id);
            if (!entry) {
                return;
            }

            if (msg.type === 'prefetchResult') {
                entry.resolve(msg.results);

                pending.delete(id);
            } else if (msg.type === 'aborted') {
                entry.reject(new Error('aborted'));

                pending.delete(id);
            } else if (msg.type === 'error') {
                entry.reject(new Error(msg.error || 'worker error'));

                pending.delete(id);
            }
        };

        worker.onerror = (e) => {
            for (const [id, p] of pending.entries()) {
                p.reject(new Error(`worker error: ${e?.message ?? 'unknown'}`));
                pending.delete(id);
            }

            try {
                worker?.terminate();
            } catch (e) {
                // 무시
            }

            worker = null;
        };

        return worker;
    } catch (err) {
        worker = null;

        return null;
    }
}

async function fallbackPrefetch(resources: Resource[], timeoutMs = 3000) {
    return Promise.all(resources.map(async (r) => {
        const out: any = {url: r.url, ok: false, status: 0};

        try {
            const controller = new AbortController();
            const to = setTimeout(() => controller.abort(), timeoutMs);
            const res = await fetch(r.url, {signal: controller.signal, credentials: 'include'});

            clearTimeout(to);
            out.status = res.status || 0;

            if (!res.ok) {
                out.ok = false;
                out.error = `HTTP ${res.status}`;
                return out;
            }

            out.data = r.responseType === 'json' ? await res.json() : await res.text();
            out.ok = true;
        } catch (err: any) {
            out.ok = false;
            out.error = err && err.message ? err.message : String(err);
        }
        return out;
    }));
}

async function prefetch(resources: Resource[], opts?: { timeoutMs?: number }) {
    const timeoutMs = opts?.timeoutMs ?? 3000;
    const w = createWorker();
    const id = `rw_${Date.now()}_${nextId++}`;

    if (!w) {
        return fallbackPrefetch(resources, timeoutMs);
    }

    return new Promise<any[]>((resolve, reject) => {
        pending.set(id, {resolve, reject});

        const timer = setTimeout(() => {
            if (pending.has(id)) {
                pending.delete(id);
                fallbackPrefetch(resources, timeoutMs).then(resolve).catch(reject);
            }
        }, timeoutMs + 500);
        pending.get(id)!.timer = timer as unknown as number;

        try {
            w.postMessage({id, type: 'prefetch', resources, timeoutMs});
        } catch (err) {
            clearTimeout(timer);
            pending.delete(id);
            fallbackPrefetch(resources, timeoutMs).then(resolve).catch(reject);
        }
    });
}

// Abort 가능한 prefetch 인터페이스
export function prefetchWithAbort(resources: Resource[], opts?: { timeoutMs?: number }) {
    const timeoutMs = opts?.timeoutMs ?? 3000;
    const id = `rw_${Date.now()}_${nextId++}`;
    const w = createWorker();

    const promise = (async () => {
        if (!w) {
            return fallbackPrefetch(resources, timeoutMs);
        }

        return new Promise<any[]>((resolve, reject) => {
            pending.set(id, {resolve, reject});

            const timer = setTimeout(() => {
                if (pending.has(id)) {
                    pending.delete(id);
                    fallbackPrefetch(resources, timeoutMs).then(resolve).catch(reject);
                }
            }, timeoutMs + 500);

            pending.get(id)!.timer = timer as unknown as number;

            try {
                w.postMessage({id, type: 'prefetch', resources, timeoutMs});
            } catch (err) {
                clearTimeout(timer);
                pending.delete(id);
                fallbackPrefetch(resources, timeoutMs).then(resolve).catch(reject);
            }
        });
    })();

    const abort = () => {
        try {
            if (w) {
                w.postMessage({id, type: 'abort'});
            }

            const entry = pending.get(id);
            if (entry) {
                entry.reject(new Error('aborted'));
                pending.delete(id);
            }
        } catch (e) {
            // 무시
        }
    };

    return {id, promise, abort};
}

// 단일 URL에 대해 파싱된 결과를 반환하는 간단한 API
export async function fetchParsed(url: string, opts?: { responseType?: 'json' | 'text'; timeoutMs?: number }) {
    const responseType = opts?.responseType ?? 'json';
    const timeoutMs = opts?.timeoutMs ?? 3000;
    const resArr = await prefetch([{url, responseType}], {timeoutMs});

    const r = Array.isArray(resArr) && resArr[0] ? resArr[0] : null;
    if (!r) {
        throw new Error('no-response');
    }

    if (!r.ok) {
        const err = new Error(r.error || `HTTP ${r.status}`);
        (err as any).status = r.status || 0;
        throw err;
    }

    return r.data;
}


// 전역 노출
declare global {
    interface Window {
        __resultsFetchParsed?: typeof fetchParsed
    }
}

if (typeof window !== 'undefined') {
    (window as any).__resultsFetchParsed = fetchParsed;
}
