#include <vector>
#include "common.hpp"
#include "filter_uninteresting_contigs.hpp"

using namespace std;

unsigned int filter_uninteresting_contigs(chimeric_alignments_t& chimeric_alignments, const vector<bool>& interesting_contigs) {
	unsigned int remaining = 0;
	for (chimeric_alignments_t::iterator chimeric_alignment = chimeric_alignments.begin(); chimeric_alignment != chimeric_alignments.end(); ++chimeric_alignment) {

		if (chimeric_alignment->second.filter != FILTER_none)
			continue; // the read has already been filtered

		// all mates must be on an interesting contig
		for (mates_t::iterator mate = chimeric_alignment->second.begin(); ; ++mate) {
			if (mate == chimeric_alignment->second.end()) {
				++remaining;
				break;
			} else if (!interesting_contigs[mate->contig]) {
				chimeric_alignment->second.filter = FILTER_uninteresting_contigs;
				break;
			}
		}
	}
	return remaining;
}

