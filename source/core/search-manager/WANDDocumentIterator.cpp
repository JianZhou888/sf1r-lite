/**
 * \file WANDDocumentIterator.cpp
 * \brief 
 * \date Feb 29, 2012
 * \author Xin Liu
 */

#include "WANDDocumentIterator.h"

using namespace sf1r;

WANDDocumentIterator::~WANDDocumentIterator()
{
    property_index_term_index_iterator prop_iter = docIteratorList_.begin();
    for (; prop_iter != docIteratorList_.end(); ++prop_iter)
    {
        term_index_dociter_iterator term_iter = (*prop_iter).begin();
        for(; term_iter != (*prop_iter).end(); ++term_iter)
        {
            if(term_iter->second)
                delete term_iter->second;
        }
    }

   // if(pDocIteratorQueue_)
    //    delete pDocIteratorQueue_;
}

void WANDDocumentIterator::add(
        propertyid_t propertyId,
        unsigned int termIndex,
        TermDocumentIterator* pDocIterator)
{
    size_t index = getIndexOfPropertyId_(propertyId);
    boost::mutex::scoped_lock lock(mutex_);
    docIteratorList_[index][termIndex] = pDocIterator;
}

size_t WANDDocumentIterator::getIndexOfPropertyId_(propertyid_t propertyId)
{
    size_t numProperties = indexPropertyIdList_.size();
    for (size_t i = 0; i < numProperties; ++i)
    {
        if(indexPropertyIdList_[i] == propertyId)
            return i;
    }
    return 0;
}

size_t WANDDocumentIterator::getIndexOfProperty_(const std::string& property)
{
    size_t numProperties = indexPropertyList_.size();
    for (size_t i = 0; i < numProperties; ++i)
    {
        if(indexPropertyList_[i] == property)
            return i;
    }
    return 0;
}

void WANDDocumentIterator::init_(const property_weight_map& propertyWeightMap)
{
    //pDocIteratorQueue_ = NULL;
    currDoc_ = 0;
    currThreshold_ = 0.0F;
    size_t numProperties = indexPropertyList_.size();
    propertyWeightList_.resize(numProperties);
    docIteratorList_.resize(numProperties);

    for (size_t i = 0; i < numProperties; ++i)
    {
        property_weight_map::const_iterator found
            = propertyWeightMap.find(indexPropertyIdList_[i]);
        if (found != propertyWeightMap.end())
        {
            propertyWeightList_[i] = found->second;
        }
        else
            propertyWeightList_[i] = 0.0F;
    }
}

void WANDDocumentIterator::set_ub(const UpperBoundInProperties& ubmap)
{
    ubmap_ = ubmap;
    property_name_term_index_iterator prop_iter = ubmap.begin();
    for( ; prop_iter != ubmap.end(); ++prop_iter )
    {
        const std::string& currentProperty = prop_iter->first;
        size_t index = getIndexOfProperty_(currentProperty);
        if (docIteratorList_.size() >= (index + 1) )
        {
            const std::map<unsigned int,TermDocumentIterator*>& termIndexDociter = docIteratorList_[index];
            term_index_ub_iterator  term_iter = (prop_iter->second).begin();
            for(; term_iter != (prop_iter->second).end(); ++term_iter )
            {
                size_t termIndex = term_iter->first;
                term_index_dociter_iterator it;
                if( (it = termIndexDociter.find(termIndex)) != termIndexDociter.end() )
                {
                    TermDocumentIterator* pDocIter = it->second;
                    pDocIter->set_ub(term_iter->second);
                    //float ub = term_iter->second;
                    //propertyid_t propertyId = indexPropertyIdList_[index];
                    //std::pair<propertyid_t, float> pro_ub = std::make_pair(propertyId, ub);
                    //dociterUb_[pDocIter] = pro_ub;
                }
            }
        }
    }
}

void WANDDocumentIterator::init_threshold(float threshold)
{
    property_name_term_index_iterator property_iter = ubmap_.begin();
    float minUB = (std::numeric_limits<float>::max) ();
    float maxUB = - minUB;
    std::string currProperty;
    size_t currIndex;
    for( ; property_iter != ubmap_.end(); ++property_iter )
    {
        currProperty = property_iter->first;
        currIndex = getIndexOfProperty_(currProperty);
        float sumUBs = 0.0F;
        term_index_ub_iterator term_iter = (property_iter->second).begin();
        for( ; term_iter != (property_iter->second).end(); ++term_iter )
        {
            sumUBs += term_iter->second;
        }
        sumUBs *= propertyWeightList_[currIndex];
        if(sumUBs > maxUB)
        {
            maxUB = sumUBs;
        }
    }
    if(threshold > 0.0F)
    {
        currThreshold_ = maxUB * threshold;
    }
    else
    {
        currThreshold_ = maxUB * 0.5;
    }

    //LOG(INFO)<<"the initial currThreshold = "<<currThreshold_;
}

void WANDDocumentIterator::set_threshold(float realThreshold)
{
    currThreshold_ = realThreshold;
    //LOG(INFO)<<"the updated threshold :"<<currThreshold_;
}

bool WANDDocumentIterator::next()
{
    return do_next();
}

void WANDDocumentIterator::initDocIteratorListSorter()
{
    if(docIteratorList_.size() < 1)
        return;
    TermDocumentIterator* pDocIterator;
    typedef std::vector<std::map<unsigned int,TermDocumentIterator*> >::iterator property_iterator;
    typedef std::map<unsigned int,TermDocumentIterator*>::iterator term_index_iterator;

    property_iterator prop_iter = docIteratorList_.begin();
    for( ; prop_iter != docIteratorList_.end(); ++prop_iter)
    {
        term_index_iterator term_iter = (*prop_iter).begin();
        for( ; term_iter != (*prop_iter).end(); term_iter++ )
        {
            pDocIterator = term_iter->second;
            if(pDocIterator)
            {
                pDocIterator->setCurrent(false);
                if(pDocIterator->next())
                {
                    docIteratorListSorter_.push_back(pDocIterator);
                }
                else
                {
                    term_iter->second = NULL;
                    delete pDocIterator;
                }
            }
        }
    }
    std::sort(docIteratorListSorter_.begin(), docIteratorListSorter_.end(), SortPred());
}

bool WANDDocumentIterator::findPivot() // multimap sorter as temporary variables
{
    float sumUB = 0.0F; //sum of upper bounds of all terms
    for(size_t i = 0; i < docIteratorListSorter_.size(); i++)
    {
        TermDocumentIterator* pDocIterator = docIteratorListSorter_[i];
        if(pDocIterator)
        {
            size_t index = getIndexOfPropertyId_(pDocIterator->propertyId_);
            sumUB += pDocIterator->ub_ * propertyWeightList_[index];
            if(sumUB > currThreshold_)
            {
                pivotDoc_ = pDocIterator->doc();
                return true;
            }
        }
    }
    return false;
}

bool WANDDocumentIterator::processPrePostings(docid_t target)//multimap sorter as temporary variables
{
    if(docIteratorListSorter_.size() < 1)
        return false;
    docid_t nFoundId = MAX_DOC_ID;
    size_t nMatch = 0;

    for(size_t i = 0; i < docIteratorListSorter_.size(); i++)
    {
        TermDocumentIterator* pEntry = docIteratorListSorter_[i];
        if(pEntry != NULL && pEntry->doc() < target)
        {
            nFoundId = pEntry->skipTo(target);
            if(MAX_DOC_ID != nFoundId)
                nMatch++;
        }
    }

    if (nMatch > 1)
        return true;
    else
        return false;
}

bool WANDDocumentIterator::bubble(size_t n)
{
    bool swapped = false;
    for(size_t i = 0; i < n-1; i++)
    {
        if (docIteratorListSorter_[i]->doc() > docIteratorListSorter_[i+1]->doc())
        {
            TermDocumentIterator* tempIter = docIteratorListSorter_[i];
            docIteratorListSorter_[i] = docIteratorListSorter_[i+1];
            docIteratorListSorter_[i+1] = tempIter;
            swapped = true;
        }
    }

    return swapped;
}

void WANDDocumentIterator::bubble_sort()//bubble sort for array items
{
    size_t size = docIteratorListSorter_.size();
    std::vector<TermDocumentIterator*> docIteratorList;
    for(size_t i = 0; i < size; i++)
    {
        TermDocumentIterator* pDocIter = docIteratorListSorter_[i];
        if(MAX_DOC_ID != pDocIter->doc())
        {
            docIteratorList.push_back(pDocIter);
        }
    }
    docIteratorListSorter_.swap(docIteratorList);
    size = docIteratorListSorter_.size();
    for(size_t i = size; i > 1 && bubble(i); i--);
}

bool WANDDocumentIterator::do_next()
{
    do
    {
        if(docIteratorListSorter_.size() == 0)
        {
            initDocIteratorListSorter();
        }
        if(docIteratorListSorter_.size() == 0)
            return false;

        for(size_t i = 0; i < docIteratorListSorter_.size(); i++)
        {
            TermDocumentIterator* pDocIter = docIteratorListSorter_[i];
            if(pDocIter != NULL && pDocIter->isCurrent())
            {
                pDocIter->setCurrent(false);
                pDocIter->next();
            }
        }

        bubble_sort();
        docid_t frontDocId = docIteratorListSorter_[0]->doc();

        if (findPivot() == false)
            return false;

        //std::cout<<std::endl<<"===the pivot document:"<<pivotDoc_<<std::endl;
        //std::cout<<"===the current doc:"<<currDoc_<<std::endl;

        if (pivotDoc_ <= currDoc_)
        {
            if(processPrePostings(currDoc_ + 1) == false)
                return false;
        }
        else //pivotTerm_ > currdoc_
        {
            if(frontDocId == pivotDoc_)
            {
                currDoc_ = pivotDoc_;

                property_index_term_index_iterator prop_iter = docIteratorList_.begin();
                for ( ; prop_iter != docIteratorList_.end(); ++prop_iter)
                {
                    term_index_dociter_iterator term_iter = (*prop_iter).begin();
                    for( ; term_iter != (*prop_iter).end(); ++term_iter )
                    {
                        TermDocumentIterator* pEntry = term_iter->second;
                        if(pEntry)
                        {
                            if (currDoc_ == pEntry->doc())
                                pEntry->setCurrent(true);
                            else
                                pEntry->setCurrent(false);
                        }
                    }
                }
                return true;
            }
            else
            {
                if(processPrePostings(pivotDoc_) == false)
                    return false;
            }
        }
    }while(true);
}

#if SKIP_ENABLED
sf1r::docid_t WANDDocumentIterator::skipTo(sf1r::docid_t target)
{
    return do_skipTo(target);
}

sf1r::docid_t WANDDocumentIterator::do_skipTo(sf1r::docid_t target)
{
    if(docIteratorListSorter_.size() == 0)
    {
        initDocIteratorListSorter();
    }
    if(docIteratorListSorter_.size() == 0)
        return false;

    property_index_term_index_iterator prop_iter = docIteratorList_.begin();
    for ( ; prop_iter != docIteratorList_.end(); ++prop_iter)
    {
        term_index_dociter_iterator term_iter = (*prop_iter).begin();
        for( ; term_iter != (*prop_iter).end(); ++term_iter )
        {
            TermDocumentIterator* pEntry = term_iter->second;
            if(pEntry)
            {
                pEntry->setCurrent(false);
            }
        }
    }

    if ( processPrePostings(target) == false )
        return MAX_DOC_ID;
    else
    {
        if(next() == true)
            return currDoc_;
        else
            return MAX_DOC_ID;
    }
}

#endif

void WANDDocumentIterator::df_cmtf(
    DocumentFrequencyInProperties& dfmap,
    CollectionTermFrequencyInProperties& ctfmap,
    MaxTermFrequencyInProperties& maxtfmap)
{
    TermDocumentIterator* pEntry;
    property_index_term_index_iterator prop_iter = docIteratorList_.begin();
    for( ; prop_iter != docIteratorList_.end(); ++prop_iter )
    {
        term_index_dociter_iterator term_iter = (*prop_iter).begin();
        for( ; term_iter != (*prop_iter).end(); ++term_iter )
        {
            pEntry = term_iter->second;
            if(pEntry)
                pEntry->df_cmtf(dfmap, ctfmap, maxtfmap);
        }
    }
}

double WANDDocumentIterator::score(
    const std::vector<RankQueryProperty>& rankQueryProperties,
    const std::vector<boost::shared_ptr<PropertyRanker> >& propertyRankers
)
{
    CREATE_PROFILER ( compute_score, "SearchManager", "doSearch_: compute score in WANDDocumentIterator");
    CREATE_PROFILER ( get_doc_item, "SearchManager", "doSearch_: get doc_item");

    TermDocumentIterator* pEntry = 0;
    double score = 0.0F;
    size_t numProperties = rankQueryProperties.size();
    for (size_t i = 0; i < numProperties; ++i)
    {
        //LOG(INFO) << "Scoring the "<<indexPropertyList_[i]<<" property";
        term_index_dociter_iterator term_iter = docIteratorList_[i].begin();
        for(; term_iter != docIteratorList_[i].end(); ++term_iter)
        {
            pEntry = term_iter->second;
            if (pEntry && pEntry->isCurrent())
            {
                //LOG(INFO) << "Current scoring the "<<indexPropertyList_[i]<<"property";
                double weight = propertyWeightList_[i];
                if (weight != 0.0F)
                {
                    rankDocumentProperty_.reset();
                    rankDocumentProperty_.resize(rankQueryProperties[i].size());
                    pEntry->doc_item(rankDocumentProperty_);

                    //START_PROFILER ( compute_score )
                    score += weight * propertyRankers[i]->getScore(
                        rankQueryProperties[i],
                        rankDocumentProperty_
                    );
                    //STOP_PROFILER ( compute_score )
                   // LOG(INFO) << "current property's sum score:"<<score;
                }
            }
        }
    }

    if (! (score < (numeric_limits<float>::max) ()))
    {
        score = (numeric_limits<float>::max) ();
    }

    return score;
}