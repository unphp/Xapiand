/** @file documentinternal.h
 * @brief Abstract base class for a document
 */
/* Copyright 2017,2018,2019 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef XAPIAN_INCLUDED_DOCUMENTINTERNAL_H
#define XAPIAN_INCLUDED_DOCUMENTINTERNAL_H

#include "xapian/document.h"
#include "xapian/intrusive_ptr.h"
#include "xapian/types.h"

#include "xapian/api/terminfo.h"
#include "xapian/api/termlist.h"
#include "xapian/backends/databaseinternal.h"
#include "xapian/common/overflow.h"

#include <map>
#include <memory>
#include <string>
#include <stdexcept>
#include <limits>

class DocumentTermList;
class DocumentValueList;
class GlassValueManager;
class HoneyValueManager;
class ValueStreamDocument;

namespace Xapian {

/// Abstract base class for a document.
class Document::Internal : public Xapian::Internal::intrusive_base {
    friend class ::DocumentTermList;
    friend class ::DocumentValueList;
    // For ensure_values_fetched():
    friend class ::GlassValueManager;
    friend class ::HoneyValueManager;
    friend class ::ValueStreamDocument;

    /// Don't allow assignment.
    void operator=(const Internal &) = delete;

    /// Don't allow copying.
    Internal(const Internal &) = delete;

    /** The document data.
     *
     *  If NULL, this hasn't been fetched or set yet.
     */
    std::unique_ptr<std::string> data;

    /** Terms in the document and their associated metadata.
     *
     *  If NULL, the terms haven't been fetched or set yet.
     *
     *  We use std::map<> rather than std::unordered_map<> because the latter
     *  invalidates existing iterators upon insert() if rehashing occurs,
     *  whereas existing iterators remain valid for std::map<>.
     */
    mutable std::unique_ptr<std::map<std::string, TermInfo>> terms;

    /** The number of distinct terms in @a terms.
     *
     *  Only valid when terms is non-NULL.
     *
     *  This may be less than terms.size() if any terms have been deleted.
     */
    mutable Xapian::termcount termlist_size;

    /** Are there any changes to term positions in @a terms?
     *
     *  If a document is read from a database, modified and then replaced at
     *  the same docid, then we can save a lot of work if we know when there
     *  are no changes to term positions, even if there are changes to terms
     *  (a common example is adding filter terms to an existing document).
     *
     *  It's OK for this to be true when there aren't any modifications (it
     *  just means that the backend can't shortcut as directly).
     */
    mutable bool positions_modified_ = false;

    /** Ensure terms have been fetched from @a database.
     *
     *  After this call, @a terms will be non-NULL.  If @a database is NULL,
     *  @a terms will be initialised to an empty map if it was NULL.
     */
    void ensure_terms_fetched() const;

    /** Ensure values have been fetched from @a database.
     *
     *  After this call, @a values will be non-NULL.  If @a database is NULL,
     *  @a values will be initialised to an empty map if it was NULL.
     */
    void ensure_values_fetched() const;

  protected:
    /** Document value slots and their contents.
     *
     *  If NULL, the values haven't been fetched or set yet.
     *
     *  We use std::map<> rather than std::unordered_map<> because the latter
     *  invalidates existing iterators upon insert() if rehashing occurs,
     *  whereas existing iterators remain valid for std::map<>.
     */
    mutable std::unique_ptr<std::map<Xapian::valueno, std::string>> values;

    /** Database this document came from.
     *
     *  If this document didn't come from a database, this will be NULL.
     */
    mutable Xapian::Internal::intrusive_ptr<const Xapian::Database::Internal> database;

    /** The document ID this document came from in @a database.
     *
     *  If this document didn't come from a database, this will be 0.
     *
     *  Note that this is the docid in the sub-database when multiple databases
     *  are being searched.
     */
    Xapian::docid did;

    /// Constructor used by subclasses.
    Internal(Xapian::Internal::intrusive_ptr<const Xapian::Database::Internal> database_,
	     Xapian::docid did_)
	: database(database_), did(did_) {}

    /// Constructor used by RemoteDocument subclass.
    Internal(const Xapian::Database::Internal* database_,
	     Xapian::docid did_,
	     std::string&& data_,
	     std::map<Xapian::valueno, std::string>&& values_)
	: data(new std::string(std::move(data_))),
	  values(new std::map<Xapian::valueno, std::string>(std::move(values_))),
	  database(database_),
	  did(did_) {}

    /** Fetch the document data from the database.
     *
     *  The default implementation (used when there's no associated database)
     *  returns an empty string.
     */
    virtual std::string fetch_data() const;

    /** Fetch all set values from the database.
     *
     *  The default implementation (used when there's no associated database)
     *  clears @a values_.
     */
    virtual void fetch_all_values(std::map<Xapian::valueno,
				  std::string>& values_) const;

    /** Fetch a single value from the database.
     *
     *  The default implementation (used when there's no associated database)
     *  returns an empty string.
     */
    virtual std::string fetch_value(Xapian::valueno slot) const;

  public:
    /// Construct an empty document.
    Internal() : did(0) {}

    /** We have virtual methods and want to be able to delete derived classes
     *  using a pointer to the base class, so we need a virtual destructor.
     */
    virtual ~Internal();

    void set_database(const Database& db) const;

    /** Return true if the document data might have been modified.
     *
     *  If the document is from a database, this means modifications
     *  compared to the version read, otherwise it means modifications
     *  compared to an empty database.
     */
    bool data_modified() const { return data != NULL; }

    /** Return true if the document's terms might have been modified.
     *
     *  If the document is from a database, this means modifications
     *  compared to the version read, otherwise it means modifications
     *  compared to an empty database.
     */
    bool terms_modified() const { return terms != NULL; }

    /** Return true if the document's values might have been modified.
     *
     *  If the document is from a database, this means modifications
     *  compared to the version read, otherwise it means modifications
     *  compared to an empty database.
     */
    bool values_modified() const { return values != NULL; }

    /** Return true if the document might have been modified in any way.
     *
     *  If the document is from a database, this means modifications
     *  compared to the version read, otherwise it means modifications
     *  compared to an empty database.
     */
    bool modified() const {
	return data_modified() || terms_modified() || values_modified();
    }

    /** Return true if the document's term positions might have been modified.
     *
     *  If the document is from a database, this means modifications
     *  compared to the version read, otherwise it means modifications
     *  compared to an empty database.
     */
    bool positions_modified() const { return positions_modified_; }

    /** Get the document ID this document came from.
     *
     *  If this document didn't come from a database, this will be 0.
     *
     *  Note that this is the docid in the sub-database when multiple databases
     *  are being searched.
     */
    Xapian::docid get_docid() const { return did; }

    /// Get the document data.
    std::string get_data() const {
	if (data)
	    return *data;
	return fetch_data();
    }

    /// Set the document data.
    void set_data(const std::string& data_) {
	data.reset(new std::string(data_));
    }

    /// Add a term to this document.
    void add_term(const std::string& term, Xapian::termcount wdf_inc) {
	ensure_terms_fetched();

	auto i = terms->find(term);
	if (i == terms->end()) {
	    ++termlist_size;
	    terms->emplace(make_pair(term, TermInfo(wdf_inc)));
	} else {
	    if (i->second.increase_wdf(wdf_inc))
		++termlist_size;
	}
    }

    /// Remove a term from this document.
    bool remove_term(const std::string& term) {
	ensure_terms_fetched();

	auto i = terms->find(term);
	if (i == terms->end()) {
	    return false;
	}
	if (i->second.has_positions()) {
	    positions_modified_ = true;
	}
	if (!i->second.remove()) {
	    return false;
	}
	--termlist_size;
	return true;
    }

    /// Add a posting for a term.
    void add_posting(const std::string& term,
		     Xapian::termpos term_pos,
		     Xapian::termcount wdf_inc) {
	ensure_terms_fetched();
	positions_modified_ = true;

	auto i = terms->find(term);
	if (i == terms->end()) {
	    ++termlist_size;
	    terms->emplace(term, TermInfo(wdf_inc, term_pos));
	    return;
	}
	if (i->second.add_position(wdf_inc, term_pos))
	    ++termlist_size;
    }

    enum remove_posting_result { OK, NO_TERM, NO_POS };

    /// Remove a posting for a term.
    remove_posting_result
    remove_posting(const std::string& term,
		   Xapian::termpos term_pos,
		   Xapian::termcount wdf_dec) {
	ensure_terms_fetched();

	auto i = terms->find(term);
	if (i == terms->end() || i->second.is_deleted()) {
	    return remove_posting_result::NO_TERM;
	}
	if (!i->second.remove_position(term_pos)) {
	    return remove_posting_result::NO_POS;
	}
	if (i->second.decrease_wdf(wdf_dec))
	    --termlist_size;
	positions_modified_ = true;
	return remove_posting_result::OK;
    }

    /** Remove a range of postings for a term.
     *
     *  Can only return OK or NO_TERM.
     */
    remove_posting_result
    remove_postings(const std::string& term,
		    Xapian::termpos term_pos_first,
		    Xapian::termpos term_pos_last,
		    Xapian::termcount wdf_dec,
		    Xapian::termpos& n_removed) {
	ensure_terms_fetched();

	auto i = terms->find(term);
	if (i == terms->end() || i->second.is_deleted()) {
	    return remove_posting_result::NO_TERM;
	}
	n_removed = i->second.remove_positions(term_pos_first,
					       term_pos_last);
	if (n_removed) {
	    positions_modified_ = true;
	    Xapian::termcount wdf_delta;
	    if (mul_overflows(n_removed, wdf_dec, wdf_delta)) {
		// Decreasing by the maximum value will zero the wdf.
		wdf_delta = numeric_limits<Xapian::termcount>::max();
	    }
	    if (i->second.decrease_wdf(wdf_delta))
		--termlist_size;
	}
	return remove_posting_result::OK;
    }

    /// Clear all terms from the document.
    void clear_terms() {
	if (!terms) {
	    if (database.get()) {
		terms.reset(new map<string, TermInfo>());
		termlist_size = 0;
	    } else {
		// We didn't come from a database, so there are no unfetched
		// terms to clear.
	    }
	} else {
	    terms->clear();
	    termlist_size = 0;
	    // Assume there was positional data if there's any in the database.
	    positions_modified_ = database.get() && database->has_positions();
	}
    }

    /// Return the number of distinct terms in this document.
    Xapian::termcount termlist_count() const {
	if (terms)
	    return termlist_size;

	if (!database.get())
	    return 0;

	std::unique_ptr<TermList> tl(database->open_term_list(did));
	// get_approx_size() is exact for TermList from a database.
	return tl->get_approx_size();
    }

    /** Start iterating the terms in this document.
     *
     *  @return A new TermList object (caller takes ownership) or NULL if
     *		there are no terms.
     */
    TermList* open_term_list() const;

    /** Read a value slot in this document.
     *
     *  @return The value in slot @a slot, or an empty string if not set.
     */
    std::string get_value(Xapian::valueno slot) const {
	if (values) {
	    auto i = values->find(slot);
	    if (i != values->end())
		return i->second;
	    return std::string();
	}

	return fetch_value(slot);
    }

    /// Add a value to a slot in this document.
    void add_value(Xapian::valueno slot, const std::string& value) {
	ensure_values_fetched();

	if (!value.empty()) {
	    (*values)[slot] = value;
	} else {
	    // Empty values aren't stored, but replace any existing value by
	    // removing it.
	    values->erase(slot);
	}
    }

    /// Clear all value slots in this document.
    void clear_values() {
	if (!values) {
	    if (database.get()) {
		values.reset(new map<Xapian::valueno, string>());
	    } else {
		// We didn't come from a database, so there are no unfetched
		// values to clear.
	    }
	} else {
	    values->clear();
	}
    }

    /// Count the value slots used in this document.
    Xapian::valueno values_count() const {
	ensure_values_fetched();
	return values->size();
    }

    Xapian::ValueIterator values_begin() const;

    /// Return a string describing this object.
    std::string get_description() const;
};

}

#endif // XAPIAN_INCLUDED_DOCUMENTINTERNAL_H
