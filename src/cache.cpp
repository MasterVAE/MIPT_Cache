#include <stdio.h>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <limits>

#include "config.h"
#include "cache.h"


struct Cache_LRU : CacheFunc
{
    size_t size;

    std::list<VALUE> values;

    std::unordered_map<VALUE, std::list<VALUE>::iterator> cache;

    Cache_LRU(size_t size) : size(size) {}

    VALUE GetValue(VALUE value) override
    {
        auto it = cache.find(value);

        if (it != cache.end())
        {
            values.splice(values.begin(), values, it->second);
           
            it->second = values.begin();

            return 0;
        }

        int miss = 0;

        if (next != NULL)
            miss += next->GetValue(value);

        miss += 1;

        values.push_front(value);
        cache[value] = values.begin();

        if (values.size() > size)
        {
            VALUE old = values.back();
            values.pop_back();
            cache.erase(old);
        }

        return miss;
    }
};

struct Cache_LFU : CacheFunc
{
    size_t size;

    std::unordered_map<VALUE, size_t> frequency;

    std::unordered_map<size_t, std::unordered_set<VALUE>> values;

    size_t min_frequency = 0;

    Cache_LFU(size_t size) : size(size) {}

    VALUE GetValue(VALUE value) override
    {
        auto it = frequency.find(value);

        if (it != frequency.end())
        {
            size_t old_frequency = it->second;

            values[old_frequency].erase(value);

            if (values[old_frequency].empty())
            {
                values.erase(old_frequency);

                if (min_frequency == old_frequency)
                    min_frequency++;
            }

            size_t new_frequency = old_frequency + 1;
            it->second = new_frequency;

            values[new_frequency].insert(value);

            return 0;
        }

        int miss = 0;

        if (next != NULL)
            miss += next->GetValue(value);

        miss += 1;

        if (frequency.size() >= size && size > 0)
        {
            auto& min_values = values[min_frequency];

            auto victim = min_values.begin();
            VALUE old_value = *victim;

            min_values.erase(victim);

            if (min_values.empty())
                values.erase(min_frequency);

            frequency.erase(old_value);
        }

        frequency[value] = 1;
        values[1].insert(value);

        min_frequency = 1;

        return miss;
    }
};

struct Cache_2Q : CacheFunc
{
    size_t size;
    size_t kin;   // размер FIFO-очереди A1in (первичные обращения)
    size_t kout;  // размер FIFO-призрака A1out

    // A1in: FIFO для первых обращений (холодные данные)
    std::list<VALUE> a1in;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> a1in_map;

    // A1out: «призрачный» FIFO-буфер — хранит ключи недавно вытесненных из A1in
    std::list<VALUE> a1out;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> a1out_map;

    // Am: основной LRU-буфер для «горячих» данных (≥ 2 обращений)
    std::list<VALUE> am;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> am_map;

    Cache_2Q(size_t size) : size(size)
    {
        // A1in ≈ 25% кэша, A1out ≈ 50% (призраки), Am = остальное
        kin = size / 4;
        if (kin == 0 && size > 0) kin = 1;
        kout = size / 2;
        if (kout == 0 && size > 0) kout = 1;
    }

    VALUE GetValue(VALUE value) override
    {
        // ── Попадание в Am (основной буфер) ──
        auto it_am = am_map.find(value);
        if (it_am != am_map.end())
        {
            // LRU: перемещаем в начало (MRU)
            am.splice(am.begin(), am, it_am->second);
            it_am->second = am.begin();
            return 0;
        }

        // ── Попадание в A1in (FIFO первых обращений) ──
        auto it_in = a1in_map.find(value);
        if (it_in != a1in_map.end())
        {
            // Повышаем: из A1in → в Am (теперь это «горячий» элемент)
            a1in.erase(it_in->second);
            a1in_map.erase(it_in);

            am.push_front(value);
            am_map[value] = am.begin();

            // Если Am переполнен — вытесняем LRU из Am
            if (am.size() > size - kin)
            {
                VALUE old = am.back();
                am.pop_back();
                am_map.erase(old);
            }

            return 0;
        }

        // ── Попадание в A1out (призрак) — повторное обращение после вытеснения ──
        auto it_out = a1out_map.find(value);
        if (it_out != a1out_map.end())
        {
            // Удаляем из призраков
            a1out.erase(it_out->second);
            a1out_map.erase(it_out);

            int miss = 0;
            if (next != NULL)
                miss += next->GetValue(value);
            miss += 1;

            // Сразу в Am — элемент доказал свою ценность
            am.push_front(value);
            am_map[value] = am.begin();

            if (am.size() > size - kin)
            {
                VALUE old = am.back();
                am.pop_back();
                am_map.erase(old);
            }

            return miss;
        }

        // ── Полный промах — элемент нигде не найден ──
        int miss = 0;
        if (next != NULL)
            miss += next->GetValue(value);
        miss += 1;

        if (size == 0) return miss;

        // Добавляем в A1in
        a1in.push_front(value);
        a1in_map[value] = a1in.begin();

        // Если A1in переполнен — вытесняем самый старый в A1out (призраки)
        if (a1in.size() > kin)
        {
            VALUE old = a1in.back();
            a1in.pop_back();
            a1in_map.erase(old);

            a1out.push_front(old);
            a1out_map[old] = a1out.begin();

            // Если A1out переполнен — забываем самый старый призрак
            if (a1out.size() > kout)
            {
                VALUE evict = a1out.back();
                a1out.pop_back();
                a1out_map.erase(evict);
            }
        }

        return miss;
    }
};

struct Cache_ARC : CacheFunc
{
    size_t size;
    size_t p;  // целевой размер T1 (адаптивный параметр)

    // T1: LRU-буфер для «недавних» элементов (первое обращение)
    std::list<VALUE> t1;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> t1_map;

    // T2: LRU-буфер для «частых» элементов (≥ 2 обращений)
    std::list<VALUE> t2;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> t2_map;

    // B1: призраки, вытесненные из T1
    std::list<VALUE> b1;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> b1_map;

    // B2: призраки, вытесненные из T2
    std::list<VALUE> b2;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> b2_map;

    Cache_ARC(size_t size) : size(size), p(0) {}

    // ─── Вспомогательный метод: вытеснение одного элемента ───
    // in_b2: true, если страница-кандидат на вставку пришла из B2
    void REPLACE(VALUE candidate, bool in_b2)
    {
        if (size == 0) return;

        // Условие вытеснения из T1:
        //   T1 не пуст И ((кандидат из B2 и |T1| == p) ИЛИ |T1| > p)
        if (!t1.empty() && ((in_b2 && t1_map.size() == p) || t1_map.size() > p))
        {
            VALUE old = t1.back();
            t1.pop_back();
            t1_map.erase(old);
            // Перемещаем в B1 (призрак T1)
            b1.push_front(old);
            b1_map[old] = b1.begin();
        }
        else
        {
            // Иначе вытесняем из T2
            VALUE old = t2.back();
            t2.pop_back();
            t2_map.erase(old);
            // Перемещаем в B2 (призрак T2)
            b2.push_front(old);
            b2_map[old] = b2.begin();
        }
    }

    VALUE GetValue(VALUE value) override
    {
        // ── Случай I: попадание в T1 или T2 ──
        auto it_t1 = t1_map.find(value);
        if (it_t1 != t1_map.end())
        {
            // Из T1 → в T2 (элемент стал «частым»)
            t1.erase(it_t1->second);
            t1_map.erase(it_t1);
            t2.push_front(value);
            t2_map[value] = t2.begin();
            return 0;
        }

        auto it_t2 = t2_map.find(value);
        if (it_t2 != t2_map.end())
        {
            // Перемещаем в MRU внутри T2
            t2.splice(t2.begin(), t2, it_t2->second);
            it_t2->second = t2.begin();
            return 0;
        }

        // ── Случай II: попадание в B1 (призрак T1) ──
        auto it_b1 = b1_map.find(value);
        if (it_b1 != b1_map.end())
        {
            // Адаптация: увеличиваем p (T1 должен быть больше — recency важнее)
            // delta = 1, если |B1| >= |B2|, иначе |B2| / |B1|
            size_t delta = 1;
            if (b1_map.size() < b2_map.size())
                delta = b2_map.size() / std::max(b1_map.size(), (size_t)1);
            p = std::min(p + delta, size);

            int miss = 0;
            if (next != NULL)
                miss += next->GetValue(value);
            miss += 1;

            REPLACE(value, false);

            // Удаляем из B1 и помещаем в T2
            b1.erase(it_b1->second);
            b1_map.erase(it_b1);
            t2.push_front(value);
            t2_map[value] = t2.begin();

            return miss;
        }

        // ── Случай III: попадание в B2 (призрак T2) ──
        auto it_b2 = b2_map.find(value);
        if (it_b2 != b2_map.end())
        {
            // Адаптация: уменьшаем p (T2 должен быть больше — frequency важнее)
            size_t delta = 1;
            if (b2_map.size() < b1_map.size())
                delta = b1_map.size() / std::max(b2_map.size(), (size_t)1);
            p = (p > delta) ? (p - delta) : 0;

            int miss = 0;
            if (next != NULL)
                miss += next->GetValue(value);
            miss += 1;

            REPLACE(value, true);

            // Удаляем из B2 и помещаем в T2
            b2.erase(it_b2->second);
            b2_map.erase(it_b2);
            t2.push_front(value);
            t2_map[value] = t2.begin();

            return miss;
        }

        // ── Случай IV: полный промах ──
        int miss = 0;
        if (next != NULL)
            miss += next->GetValue(value);
        miss += 1;

        if (size == 0) return miss;

        // ── Инвариант: |T1|+|T2|+|B1|+|B2| ≤ 2c ──
        size_t total = t1_map.size() + t2_map.size() + b1_map.size() + b2_map.size();
        if (total >= 2 * size)
        {
            if (!b1.empty())
            {
                VALUE old = b1.back();
                b1.pop_back();
                b1_map.erase(old);
            }
            else if (!b2.empty())
            {
                VALUE old = b2.back();
                b2.pop_back();
                b2_map.erase(old);
            }
        }

        // ── Инвариант: |T1|+|T2| ≤ c ──
        if (t1_map.size() + t2_map.size() >= size)
        {
            // Случай A: |T1|+|B1| == c
            if (t1_map.size() + b1_map.size() == size)
            {
                if (t1_map.size() < size)
                {
                    // Удаляем LRU из B1
                    VALUE old = b1.back();
                    b1.pop_back();
                    b1_map.erase(old);
                    REPLACE(value, false);
                }
                else
                {
                    // B1 пуст — удаляем LRU из T1 без сохранения в B1
                    VALUE old = t1.back();
                    t1.pop_back();
                    t1_map.erase(old);
                }
            }
            else
            {
                // Случай B: |T1|+|B1| < c
                REPLACE(value, false);
            }
        }

        // Добавляем новый элемент в T1
        t1.push_front(value);
        t1_map[value] = t1.begin();

        return miss;
    }
};

struct Cache_LIRS : CacheFunc
{
    size_t size;
    size_t lir_capacity;  // целевое количество LIR-блоков (≈ 99% кэша)
    size_t hir_capacity;  // целевое количество резидентных HIR-блоков (≈ 1% кэша)

    // S: LRU-стек ВСЕХ блоков, к которым были обращения (резидентные и нерезидентные)
    std::list<VALUE> stack;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> stack_map;

    // LIR-блоки (Low IRR) — всегда резидентны, основная часть кэша
    std::unordered_set<VALUE> lir_set;

    // Q: LRU-очередь резидентных HIR-блоков (High IRR)
    std::list<VALUE> hir_queue;
    std::unordered_map<VALUE, std::list<VALUE>::iterator> hir_map;

    // Нерезидентные HIR-блоки: есть в S, но вытеснены из кэша
    std::unordered_set<VALUE> nonresident_hir;

    Cache_LIRS(size_t size) : size(size)
    {
        // LIRS делит кэш: ~99% LIR, ~1% резидентные HIR
        hir_capacity = size / 100;
        if (hir_capacity == 0 && size > 0) hir_capacity = 1;
        lir_capacity = (size > hir_capacity) ? (size - hir_capacity) : 0;
    }

    // ─── Проверка статуса блока ───
    bool is_resident(VALUE v) const
    {
        return lir_set.count(v) || hir_map.count(v);
    }

    bool is_lir(VALUE v) const
    {
        return lir_set.count(v) > 0;
    }

    // ─── Переместить блок на вершину стека S (MRU) ───
    void move_to_top(VALUE value)
    {
        auto it = stack_map.find(value);
        if (it != stack_map.end())
        {
            stack.erase(it->second);
        }
        stack.push_front(value);
        stack_map[value] = stack.begin();
    }

    // ─── Обрезка стека: удаляем нерезидентные HIR с дна S ───
    // Останавливаемся, когда на дне оказывается резидентный LIR-блок
    void prune_stack()
    {
        while (!stack.empty())
        {
            VALUE bottom = stack.back();
            if (is_lir(bottom) && is_resident(bottom))
                break;

            // Нерезидентный HIR — удаляем из всех структур
            if (nonresident_hir.count(bottom))
            {
                nonresident_hir.erase(bottom);
            }
            stack_map.erase(bottom);
            stack.pop_back();
        }
    }

    // ─── Гарантировать, что общее число резидентных блоков ≤ size ───
    // Предпочитаем вытеснять из HIR-очереди; если она пуста — дно LIR
    void ensure_capacity()
    {
        while (lir_set.size() + hir_map.size() > size)
        {
            if (!hir_queue.empty())
            {
                // Вытесняем LRU резидентного HIR → нерезидентный HIR
                VALUE evict = hir_queue.back();
                hir_queue.pop_back();
                hir_map.erase(evict);
                nonresident_hir.insert(evict);
            }
            else
            {
                // Вытесняем самый нижний LIR-блок из стека → нерезидентный HIR
                for (auto rit = stack.rbegin(); rit != stack.rend(); ++rit)
                {
                    if (lir_set.count(*rit))
                    {
                        VALUE evict = *rit;
                        lir_set.erase(evict);
                        nonresident_hir.insert(evict);
                        break;
                    }
                }
            }
        }
    }

    VALUE GetValue(VALUE value) override
    {
        bool in_stack = stack_map.count(value) > 0;
        bool resident = is_resident(value);
        bool lir = is_lir(value);

        // ── Случай 1: Попадание в резидентный LIR-блок ──
        if (in_stack && resident && lir)
        {
            move_to_top(value);
            prune_stack();
            return 0;
        }

        // ── Случай 2: Попадание в резидентный HIR-блок ──
        //    Блок доказал низкий IRR → повышается до LIR
        if (in_stack && resident && !lir)
        {
            // Удаляем из HIR-очереди
            hir_queue.erase(hir_map[value]);
            hir_map.erase(value);

            move_to_top(value);

            // Повышаем до LIR
            lir_set.insert(value);

            // Понижаем самый нижний LIR до резидентного HIR
            for (auto rit = stack.rbegin(); rit != stack.rend(); ++rit)
            {
                if (lir_set.count(*rit))
                {
                    VALUE demoted = *rit;
                    lir_set.erase(demoted);
                    hir_queue.push_front(demoted);
                    hir_map[demoted] = hir_queue.begin();
                    break;
                }
            }

            prune_stack();
            ensure_capacity();
            return 0;
        }

        // ── Случай 3: Промах — нерезидентный HIR (есть в S, но не в кэше) ──
        if (in_stack && !resident)
        {
            int miss = 0;
            if (next != NULL)
                miss += next->GetValue(value);
            miss += 1;

            nonresident_hir.erase(value);
            move_to_top(value);

            // Делаем резидентным HIR
            hir_queue.push_front(value);
            hir_map[value] = hir_queue.begin();

            ensure_capacity();
            prune_stack();
            return miss;
        }

        // ── Случай 4: Полный промах — блока нет нигде ──
        int miss = 0;
        if (next != NULL)
            miss += next->GetValue(value);
        miss += 1;

        if (size == 0) return miss;

        // Добавляем в стек S
        stack.push_front(value);
        stack_map[value] = stack.begin();

        if (lir_set.size() < lir_capacity)
        {
            // Есть место в LIR — добавляем как LIR
            lir_set.insert(value);
        }
        else
        {
            // LIR заполнен — добавляем как резидентный HIR
            hir_queue.push_front(value);
            hir_map[value] = hir_queue.begin();
        }

        ensure_capacity();
        return miss;
    }
};


Cache* CreateCache(Config* config)
{
    if(!config) return NULL;

    Cache* cache = (Cache*)calloc(1, sizeof(Cache));
    if(!cache) return NULL;

    cache->layers_count = config->layers_count;
    cache->layers = (CacheFunc**)calloc(cache->layers_count, sizeof(CacheFunc*));

    for(size_t i = 0; i < cache->layers_count; i++)
    {
        switch (config->layers[i].type)
        {
        case LAYER_ARC:
            cache->layers[i] = new Cache_ARC(config->layers[i].size);;
            break;
        case LAYER_2Q:
            cache->layers[i] = new Cache_2Q(config->layers[i].size);;
            break;
        case LAYER_LFU:
            cache->layers[i] = new Cache_LFU(config->layers[i].size);;
            break;
        case LAYER_LRU:
            cache->layers[i] = new Cache_LRU(config->layers[i].size);;
            break;
        case LAYER_LIRS:
            cache->layers[i] = new Cache_LIRS(config->layers[i].size);;
            break; 
        default:
            break;
        }
    }

    for(size_t i = 0; i < cache->layers_count - 1; i++)
    {
        cache->layers[i]->next = cache->layers[i + 1];
    }
    cache->layers[cache->layers_count - 1]->next = NULL;

    return cache;
}

void DestroyCache(Cache* cache)
{
    if(!cache) return;

    for(size_t i = 0; i < cache->layers_count; i++)
    {
        delete cache->layers[i];
    }
    free(cache->layers);
    free(cache);
}

int RunCache(Cache* cache, size_t count, VALUE* numbers)
{
    if(!cache) return -1;

    size_t miss = 0;

    for(size_t i = 0; i < count; i++)
    {
        miss += cache->layers[0]->GetValue(numbers[i]);
    }

    return (int)miss;
}