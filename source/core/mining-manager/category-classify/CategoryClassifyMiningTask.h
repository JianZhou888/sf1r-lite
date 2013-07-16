/**
 * @file CategoryClassifyMiningTask.h
 * @brief mining task to classify category for each docid.
 */

#ifndef SF1R_CATEGORY_CLASSIFY_MINING_TASK_H
#define SF1R_CATEGORY_CLASSIFY_MINING_TASK_H

#include "../MiningTask.h"
#include <string>

namespace sf1r
{
class DocumentManager;
class CategoryClassifyTable;

class CategoryClassifyMiningTask : public MiningTask
{
public:
    CategoryClassifyMiningTask(
        DocumentManager& documentManager,
        CategoryClassifyTable& classifyTable,
        const std::string& targetCategoryPropName);

    virtual bool buildDocument(docid_t docID, const Document& doc);

    virtual bool preProcess();

    virtual bool postProcess();

    virtual docid_t getLastDocId() { return startDocId_; }

private:
    bool ruleByTargetCategory_(
        const Document& doc,
        std::string& targetCategory,
        std::string& classifyCategory);

    bool ruleByOriginalCategory_(
        const Document& doc,
        std::string& classifyCategory);

    bool ruleBySource_(
        const Document& doc,
        const std::string& targetCategory,
        std::string& classifyCategory);

    bool classifyByTitle_(
        std::string& title,
        std::string& classifyCategory);

private:
    DocumentManager& documentManager_;

    CategoryClassifyTable& classifyTable_;

    const std::string targetCategoryPropName_;

    docid_t startDocId_;
};

} // namespace sf1r

#endif // SF1R_CATEGORY_CLASSIFY_MINING_TASK_H
