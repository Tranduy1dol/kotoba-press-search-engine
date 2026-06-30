#include "benchmark/corpus_generator.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_set>

namespace benchmark {

namespace {

uint64_t MixSeed(uint64_t a, uint64_t b) {
  return a * 6364136223846793005ULL + b + 1;
}

}  // namespace

CorpusGenerator::CorpusGenerator(uint64_t seed, uint64_t vocabulary_size,
                                 uint64_t avg_doc_length, uint32_t num_topics)
    : seed_(seed),
      vocabulary_size_(vocabulary_size),
      avg_doc_length_(avg_doc_length),
      num_topics_(num_topics) {
  vocabulary_.reserve(vocabulary_size_);
  for (uint64_t i = 0; i < vocabulary_size_; ++i) {
    vocabulary_.push_back(GenerateTerm(i));
  }
  topic_terms_.reserve(num_topics_);
  for (uint32_t t = 0; t < num_topics_; ++t) {
    topic_terms_.push_back(vocabulary_[t % vocabulary_.size()]);
  }
}

std::string CorpusGenerator::GenerateTerm(uint64_t index) const {
  std::ostringstream oss;
  oss << "term" << index;
  return oss.str();
}

std::vector<std::string> CorpusGenerator::SampleTerms(uint64_t count,
                                                      uint64_t doc_seed) const {
  std::mt19937_64 rng(MixSeed(seed_, doc_seed));
  std::vector<std::string> terms;
  terms.reserve(count);

  uint32_t topic_id =
      static_cast<uint32_t>(doc_seed % static_cast<uint64_t>(num_topics_));
  const std::string& topic_term = topic_terms_[topic_id];

  size_t topic_slots = std::max<size_t>(1, count / 5);
  for (size_t i = 0; i < topic_slots; ++i) {
    terms.push_back(topic_term);
  }

  std::uniform_int_distribution<uint64_t> dist(0, vocabulary_.size() - 1);
  while (terms.size() < count) {
    terms.push_back(vocabulary_[dist(rng)]);
  }

  std::shuffle(terms.begin(), terms.end(), rng);
  return terms;
}

std::vector<CorpusDocument> CorpusGenerator::Generate(
    uint64_t num_documents) const {
  std::vector<CorpusDocument> corpus;
  corpus.reserve(num_documents);

  std::mt19937_64 len_rng(seed_ ^ 0xDEADBEEF);
  std::normal_distribution<double> length_dist(
      static_cast<double>(avg_doc_length_),
      static_cast<double>(avg_doc_length_) * 0.2);

  for (uint64_t i = 0; i < num_documents; ++i) {
    CorpusDocument doc;
    doc.doc_id = static_cast<uint32_t>(i + 1);
    doc.url = "https://benchmark.local/doc/" + std::to_string(i + 1);
    doc.title = "Document " + std::to_string(i + 1);

    uint32_t topic_id =
        static_cast<uint32_t>(i % static_cast<uint64_t>(num_topics_));
    doc.topics.push_back(topic_terms_[topic_id]);

    int64_t doc_len = static_cast<int64_t>(std::round(length_dist(len_rng)));
    doc_len = std::max<int64_t>(10, std::min<int64_t>(doc_len, 2000));

    auto terms = SampleTerms(static_cast<uint64_t>(doc_len), i);
    std::ostringstream text;
    for (size_t t = 0; t < terms.size(); ++t) {
      if (t > 0) text << ' ';
      text << terms[t];
    }
    doc.text = text.str();
    corpus.push_back(std::move(doc));
  }

  return corpus;
}

CorpusStats CorpusGenerator::ComputeStats(
    const std::vector<CorpusDocument>& corpus) const {
  CorpusStats stats;
  stats.num_documents = corpus.size();
  if (corpus.empty()) {
    return stats;
  }

  std::unordered_set<std::string> unique;
  uint64_t total_tokens = 0;

  for (const auto& doc : corpus) {
    std::istringstream iss(doc.text);
    std::string token;
    uint64_t doc_tokens = 0;
    while (iss >> token) {
      unique.insert(token);
      doc_tokens++;
    }
    total_tokens += doc_tokens;
  }

  stats.total_tokens = total_tokens;
  stats.unique_terms = unique.size();
  stats.vocabulary_size = vocabulary_.size();
  stats.avg_tokens_per_document =
      static_cast<double>(total_tokens) / static_cast<double>(corpus.size());
  stats.avg_document_length = stats.avg_tokens_per_document;
  return stats;
}

}  // namespace benchmark
