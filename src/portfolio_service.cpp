#include "portfolio_service.hpp"
#include "storage.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
namespace moexwatch {
void PortfolioService::load() {
    const auto before = storage_.read_text("portfolio_journal_v2.csv");
    auto next = storage_.load_portfolio();
    file_ = std::make_unique<GuardedFile>(storage_.root() / "portfolio_journal_v2.csv");
    const auto after = file_->read();
    if ((before && before != after) ||
        (!before && after && *after != storage::serialize_portfolio_journal(next)))
        throw FileConflict("Журнал изменён во время загрузки; повторите запуск");
    journal_ = std::move(next);
    readable_ = true;
}
std::optional<PortfolioSummary> PortfolioService::calculate(const std::vector<Quote> &quotes) const {
    return readable_ ? calculate_portfolio(journal_.transactions, quotes) : std::optional<PortfolioSummary>{};
}
void PortfolioService::commit(PortfolioJournal next) {
    if (!readable_ || !valid_portfolio_journal(next))
        throw std::runtime_error("Изменение нарушает формат или связи журнала");
    file_->write(storage::serialize_portfolio_journal(next));
    journal_ = std::move(next);
    ++revision_;
}
void PortfolioService::import_transactions(std::vector<PortfolioTransaction> rows, std::uint64_t expected) {
    if (expected != revision_)
        throw std::runtime_error("Журнал изменился после предпросмотра. Повторите импорт");
    std::unordered_map<std::string, const PortfolioTransaction *> imported;
    for (const auto &row : rows)
        if (!imported.emplace(row.id, &row).second)
            throw std::runtime_error("Повторяется ID операции");
    for (const auto &old : journal_.transactions) {
        const auto it = imported.find(old.id);
        const auto *p = it == imported.end() ? nullptr : it->second;
        if (!p || p->position_id != old.position_id || p->date != old.date || p->ticker != old.ticker ||
            p->side != old.side || p->quantity != old.quantity || p->price != old.price ||
            p->commission != old.commission || p->sequence != old.sequence)
            throw std::runtime_error("Импорт не должен менять существующие операции");
    }
    auto next = journal_;
    next.transactions = std::move(rows);
    for (const auto &row : next.transactions)
        next.sequence_watermark = std::max(next.sequence_watermark, row.sequence);
    commit(std::move(next));
}
PortfolioJournal PortfolioService::with_operation(PortfolioTransaction operation) const {
    if (!valid_portfolio_transaction(operation))
        throw std::runtime_error("Некорректные поля операции");
    auto next = journal_;
    next.transactions.push_back(std::move(operation));
    next.transactions = identify_portfolio(std::move(next.transactions), next.sequence_watermark);
    for (const auto &row : next.transactions)
        next.sequence_watermark = std::max(next.sequence_watermark, row.sequence);
    return next;
}
PortfolioJournal PortfolioService::without_operation(std::string_view id) const {
    auto next = journal_;
    const auto it = std::find_if(next.transactions.begin(), next.transactions.end(),
                                 [&](const auto &row) { return row.id == id; });
    if (it == next.transactions.end())
        throw std::runtime_error("Операция уже отсутствует");
    next.transactions.erase(it);
    next.transactions = identify_portfolio(std::move(next.transactions), next.sequence_watermark);
    return next;
}
void PortfolioService::demo(PortfolioJournal journal) {
    journal_ = std::move(journal);
    readable_ = true;
    ++revision_;
}
} // namespace moexwatch
