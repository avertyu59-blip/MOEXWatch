#pragma once
#include "app_storage.hpp"
#include "guarded_file.hpp"
#include "portfolio_calculations.hpp"
#include <memory>
namespace moexwatch {
// Typed journal ownership and mutations; independent of QObject/QML.
class PortfolioService {
  public:
    explicit PortfolioService(const AppStorage &storage) : storage_(storage) {}
    void load();
    const PortfolioJournal &journal() const { return journal_; }
    std::uint64_t revision() const { return revision_; }
    bool readable() const { return readable_; }
    std::optional<PortfolioSummary> calculate(const std::vector<Quote> &quotes) const;
    void commit(PortfolioJournal next);
    void import_transactions(std::vector<PortfolioTransaction> rows, std::uint64_t expected_revision);
    PortfolioJournal with_operation(PortfolioTransaction operation) const;
    PortfolioJournal without_operation(std::string_view id) const;
    void demo(PortfolioJournal journal);

  private:
    AppStorage storage_;
    std::unique_ptr<GuardedFile> file_;
    PortfolioJournal journal_;
    std::uint64_t revision_{};
    bool readable_{};
};
} // namespace moexwatch
