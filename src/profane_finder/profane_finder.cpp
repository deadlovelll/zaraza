#include "profane_finder/profane_finder.hpp"

#include <algorithm>
#include <functional>
#include <utility>

std::vector<SourceSpan> ProfaneFinder::find_spans(std::string_view text, tf::Executor* executor) {
    if (executor != nullptr && worth_splitting(text.size())) {
        return find_spans_parallel(text, *executor);
    }
    return find_spans_serial(text);
}

// One chunk is serial work plus the pool's overhead
bool ProfaneFinder::worth_splitting(std::size_t size) {
    return size >= 2 * min_chunk_bytes;
}

std::vector<SourceSpan> ProfaneFinder::find_spans_parallel(std::string_view text, tf::Executor& executor) {
    std::vector<TextChunk> chunks = split_chunks(text, executor.num_workers());
    std::vector<std::vector<SourceSpan>> found(chunks.size());
    tf::Taskflow taskflow;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        taskflow.emplace(std::bind_front(
            &ProfaneFinder::find_chunk_spans,
            this,
            text,
            std::cref(chunks[i]),
            std::ref(found[i])
        ));
    }
    if (executor.this_worker_id() < 0) {
        executor.run(taskflow).get();
    } else {
        executor.corun(taskflow);
    }
    std::vector<SourceSpan> spans;
    for (const std::vector<SourceSpan>& part : found) {
        for (const SourceSpan& span : part) {
            if (spans.empty() || span.start >= spans.back().end) {
                spans.push_back(span);
            }
        }
    }
    return spans;
}

std::vector<TextChunk> ProfaneFinder::split_chunks(std::string_view text, std::size_t workers) {
    std::size_t step = std::max(text.size() / (workers * chunks_per_thread), min_chunk_bytes);
    std::vector<TextChunk> chunks;
    for (std::size_t owned_begin = 0; owned_begin < text.size(); owned_begin += step) {
        std::size_t owned_end = std::min(owned_begin + step, text.size());
        std::size_t begin = owned_begin - std::min(owned_begin, chunk_overlap_bytes);
        std::size_t end = std::min(owned_end + chunk_overlap_bytes, text.size());
        chunks.push_back({begin, end, owned_begin, owned_end});
    }
    return chunks;
}

void ProfaneFinder::find_chunk_spans(
    std::string_view text,
    const TextChunk& chunk,
    std::vector<SourceSpan>& spans
) {
    for (SourceSpan span : find_spans_serial(text.substr(chunk.begin, chunk.end - chunk.begin))) {
        span.start += chunk.begin;
        span.end += chunk.begin;
        if (span.start >= chunk.owned_begin && span.start < chunk.owned_end) {
            spans.push_back(span);
        }
    }
}

std::vector<SourceSpan> ProfaneFinder::find_spans_serial(std::string_view text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, false);
    if (search.profane_tokens.empty()) {
        return {};
    }
    return unicode_normalizer.map_to_source(text, std::move(search.profane_tokens), search.batch_starts);
}

bool ProfaneFinder::contains_profanity(std::string_view text) {
    ProfaneSearch search;
    find_profane_tokens(text, search, true);
    return !search.profane_tokens.empty();
}

std::size_t ProfaneFinder::censor_inplace(std::span<char> text, tf::Executor* executor) {
    std::vector<SourceSpan> spans = find_spans(std::string_view(text.data(), text.size()), executor);
    if (spans.empty()) {
        return text.size();
    }
    return swear_word_hider.hide_tokens_inplace(text, spans);
}

void ProfaneFinder::find_profane_tokens(
    std::string_view text,
    ProfaneSearch& search,
    bool first_only
) {
    const std::size_t lookahead = 131072;
    std::string normalized;
    std::vector<Token> tokens;
    std::size_t pos = 0;
    std::size_t from = 0;
    std::size_t ahead = lookahead;
    while (pos < text.size() || from < normalized.size()) {
        while (pos < text.size() && normalized.size() < from + ahead) {
            pos = unicode_normalizer.normalize_batch(text, pos, normalized);
            if (pos < text.size()) {
                search.batch_starts.push_back({pos, search.dropped + normalized.size()});
            }
        }
        std::size_t kept = tokens.size();
        std::size_t next = tokenizer.tokenize_batch(normalized, from, tokens);
        if (pos < text.size() && next == normalized.size()) {
            tokens.resize(kept);
            ahead *= 2;
            continue;
        }
        ahead = lookahead;
        swear_word_hider.find_profane_tokens(normalized, tokens, trie, search);
        if (first_only && !search.profane_tokens.empty()) {
            return;
        }
        from = next;
        if (pos == text.size()) {
            continue;
        }
        std::size_t cut = tokens.empty() ? next : tokens.front().start;
        normalized.erase(0, cut);
        for (Token& token : tokens) {
            token.start -= cut;
            token.end -= cut;
        }
        from -= cut;
        search.dropped += cut;
    }
}
