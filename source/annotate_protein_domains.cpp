#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "common.hpp"
#include "annotation.hpp"
#include "assembly.hpp"
#include "read_compressed_file.hpp"
#include "annotate_protein_domains.hpp"

using namespace std;

bool get_gff3_attribute(const string& attributes, const string& attribute_name, string& attribute_value) {

	// find start of attribute
	size_t start = attributes.find(attribute_name + "=");
	if (start >= attributes.size()) {
		cerr << "WARNING: failed to extract " << attribute_name << " from line in GFF3 file: " << attributes << endl;
		return false;
	}
	start += attribute_name.size() + 1; // move to position after "="

	// find end of attribute
	size_t end = attributes.find(';', start);
	if (end >= attributes.size())
		end = attributes.size();

	attribute_value = attributes.substr(start, end - start);
	return true;
}

void load_protein_domains(const string& filename, const contigs_t& contigs, const gene_annotation_t& gene_annotation, const unordered_map<string,gene_t>& gene_names, protein_domain_annotation_t& protein_domain_annotation, protein_domain_annotation_index_t& protein_domain_annotation_index) {

	// make a map of gene id -> gene
	unordered_map<string,gene_t> gene_ids;
	for (auto gene = gene_annotation.begin(); gene != gene_annotation.end(); ++gene)
		gene_ids[strip_ensembl_version_number(gene->gene_id)] = (gene_t) &(*gene);

	// read lines from GFF3 file
	autodecompress_file_t gff3_file(filename);
	string line;
	set<string> unknown_genes;
	while (gff3_file.getline(line)) {
		if (!line.empty() && line[0] != '#') { // skip comment lines

			tsv_stream_t tsv(line);
			protein_domain_annotation_record_t protein_domain;
			string contig, strand, attributes, gene_name, gene_id, trash;

			// parse line
			tsv >> contig >> trash >> trash >> protein_domain.start >> protein_domain.end >> trash >> strand >> trash >> attributes;
			if (tsv.fail() || contig.empty() || strand.empty() || attributes.empty()) {
				cerr << "WARNING: failed to parse line in GFF3 file: " << line << endl;
				continue;
			}

			// extract gene name and protein domain name from attributes
			if (!get_gff3_attribute(attributes, "gene_name", gene_name) ||
			    !get_gff3_attribute(attributes, "gene_id", gene_id) ||
			    !get_gff3_attribute(attributes, "Name", protein_domain.name))
				continue;

			// convert string representation of contig to numeric ID
			contigs_t::const_iterator find_contig_by_name = contigs.find(removeChr(contig));
			if (find_contig_by_name == contigs.end()) {
				cerr << "WARNING: unknown contig: " << contig << endl;
				continue;
			}

			// decode hex representations of special characters (e.g., "%2C" for ",")
			for (string::size_type pos = protein_domain.name.find("%"); pos < protein_domain.name.size(); pos = protein_domain.name.find("%", pos+1)) {
				if (pos + 2 < protein_domain.name.size()) {
					char c1 = protein_domain.name[pos+1];
					char c2 = protein_domain.name[pos+2];
					if ((c1 >= '0' && c1 <= '9' || c1 >= 'a' && c1 <= 'f' || c1 >= 'A' && c1 <= 'F') &&
					    (c2 >= '0' && c2 <= '9' || c2 >= 'a' && c2 <= 'f' || c2 >= 'A' && c2 <= 'F')) {
						istringstream iss(protein_domain.name.substr(pos+1, 2));
						unsigned int c;
						iss >> hex >> c;
						protein_domain.name = protein_domain.name.substr(0, pos) + ((char) c) + protein_domain.name.substr(pos + 3);
					}
				}
			}
			// replace white space, non-printable characters, commas and pipes with underscores
			// (the latter two because they have a special meaning in the output format)
			for (string::size_type pos = 0; pos < protein_domain.name.size(); ++pos)
				if (protein_domain.name[pos] < '!' || protein_domain.name[pos] > '~' || protein_domain.name[pos] == ',' || protein_domain.name[pos] == '|')
					protein_domain.name[pos] = '_';

			// map protein domain to gene by gene ID or name
			auto find_gene_by_id = gene_ids.find(strip_ensembl_version_number(gene_id));
			if (find_gene_by_id == gene_ids.end()) {
				auto find_gene_by_name = gene_names.find(gene_name);
				if (find_gene_by_name == gene_names.end()) {
					if (unknown_genes.find(gene_name + " " + gene_id) == unknown_genes.end()) {
						cerr << "WARNING: unknown gene: " << gene_name << " " << gene_id << endl;
						unknown_genes.insert(gene_name + " " + gene_id); // report an unknown gene only once
					}
					continue;
				} else {
					protein_domain.gene = find_gene_by_name->second;
				}
			} else {
				protein_domain.gene = find_gene_by_id->second;
			}

			// make annotation record
			protein_domain.contig = find_contig_by_name->second;
			protein_domain.start--; // GFF3 files are one-based
			protein_domain.end--; // GFF3 files are one-based
			protein_domain.strand = (strand[0] == '+') ? FORWARD : REVERSE;
			protein_domain_annotation.push_back(protein_domain);
		}
	}

	crash(protein_domain_annotation.empty(), "failed to parse GFF3 file");

	// index domains by coordinate
	make_annotation_index(protein_domain_annotation, protein_domain_annotation_index);
}

string annotate_retained_protein_domains(const contig_t contig, const position_t breakpoint, const strand_t predicted_strand, const bool predicted_strand_ambiguous, const gene_t gene, const direction_t direction, const protein_domain_annotation_index_t& protein_domain_annotation_index) {

	if (!gene->is_protein_coding)
		return "";

	if (predicted_strand_ambiguous || predicted_strand != gene->strand)
		return "";

	if (contig >= protein_domain_annotation_index.size())
		return "";

	// determine part of gene that is retained in the fusion
	position_t start = (direction == UPSTREAM) ? breakpoint : gene->start;
	position_t end = (direction == UPSTREAM) ? gene->end : breakpoint;

	// find all protein domains located in the retained part of gene
	map< string/*domain name*/, pair<unsigned int/*length*/,unsigned int/*retained bases*/> > retained_protein_domains;
	for (auto protein_domains = protein_domain_annotation_index[contig].lower_bound(start); protein_domains != protein_domain_annotation_index[contig].end() && protein_domains->first <= end; ++protein_domains)
		for (auto protein_domain = protein_domains->second.begin(); protein_domain != protein_domains->second.end(); ++protein_domain) {
			if ((**protein_domain).gene == gene) {
				unsigned int length = (**protein_domain).end - (**protein_domain).start + 1;
				unsigned int retained_bases;
				if (direction == UPSTREAM)
					retained_bases = (**protein_domain).end - max((**protein_domain).start, breakpoint) + 1;
				else
					retained_bases = min((**protein_domain).end, breakpoint) - (**protein_domain).start + 1;
				pair<unsigned int,unsigned int>& retained_protein_domain = retained_protein_domains[(**protein_domain).name];
				retained_protein_domain.first += length;
				retained_protein_domain.second += retained_bases;
			}
		}

	// concatenate tags to comma-separated string
	string result;
	for (auto protein_domain = retained_protein_domains.begin(); protein_domain != retained_protein_domains.end(); ++protein_domain) {
		if (!result.empty())
			result += ",";
		result += protein_domain->first + "(" + to_string(static_cast<long long int>(protein_domain->second.second * 100 / protein_domain->second.first)) + "%)";
	}
	return result;
}

char dna_to_protein(const string& triplet) {
	string t = triplet;
	std::transform(t.begin(), t.end(), t.begin(), (int (*)(int))std::toupper);
	string d = t.substr(0, 2);
	if (d == "GC") { return 'A'; }
	else if (t == "TGT" || t == "TGC") { return 'C'; }
	else if (t == "GAT" || t == "GAC") { return 'D'; }
	else if (t == "GAA" || t == "GAG") { return 'E'; }
	else if (t == "TTT" || t == "TTC") { return 'F'; }
	else if (d == "GG") { return 'G'; }
	else if (t == "CAT" || t == "CAC") { return 'H'; }
	else if (t == "ATT" || t == "ATC" || t == "ATA") { return 'I'; }
	else if (t == "AAA" || t == "AAG") { return 'K'; }
	else if (d == "CT" || t == "TTA" || t == "TTG") { return 'L'; }
	else if (t == "ATG") { return 'M'; }
	else if (t == "AAT" || t == "AAC") { return 'N'; }
	else if (d == "CC") { return 'P'; }
	else if (t == "CAA" || t == "CAG") { return 'Q'; }
	else if (d == "CG" || t == "AGA" || t == "AGG") { return 'R'; }
	else if (d == "TC" || t == "AGT" || t == "AGC") { return 'S'; }
	else if (d == "AC") { return 'T'; }
	else if (d == "GT") { return 'V'; }
	else if (t == "TGG") { return 'W'; }
	else if (t == "TAT" || t == "TAC") { return 'Y'; }
	else if (t == "TAA" || t == "TAG" || t == "TGA") { return '*'; }
	else { return '?'; }
}

// determines reading frame of first base of given transcript based on coding exons overlapping the transcript
int get_reading_frame(const vector<position_t>& transcribed_bases, const int from, const int to, const transcript_t transcript, const gene_t gene, const assembly_t& assembly, exon_t& exon_with_start_codon) {

	// find exon containing start codon
	if (transcript == NULL)
		exon_with_start_codon = NULL;
	else
		exon_with_start_codon = (gene->strand == FORWARD) ? transcript->first_exon : transcript->last_exon;
	while (exon_with_start_codon != NULL && exon_with_start_codon->coding_region_start == -1)
		exon_with_start_codon = (gene->strand == FORWARD) ? exon_with_start_codon->next_exon : exon_with_start_codon->previous_exon;
	if (exon_with_start_codon == NULL)
		return -1; // no coding exon found => is a non-coding transcript

	// check if there is a start codon at the start of first coding exon, otherwise the annotation is incorrect
	string first_codon;
	if (gene->strand == FORWARD)
		first_codon = assembly.at(gene->contig).substr(exon_with_start_codon->coding_region_start, 3);
	else // gene->strand == REVERSE
		first_codon = dna_to_reverse_complement(assembly.at(gene->contig).substr(exon_with_start_codon->coding_region_end - 2, 3));
	if (first_codon != "ATG")
		return -1;

	// find a coding exon that is transcribed to determine reading frame
	int reading_frame = -1;
	position_t transcribed_coding_base = -1;
	for (exon_t exon = exon_with_start_codon; exon != NULL && transcribed_coding_base == -1; exon = (gene->strand == FORWARD) ? exon->next_exon : exon->previous_exon) {
		for (int position = from; position <= to && transcribed_coding_base == -1; position++)
			if (exon->coding_region_start <= transcribed_bases[position] && exon->coding_region_end >= transcribed_bases[position])
				transcribed_coding_base = position;
		if (transcribed_coding_base == -1) {
			reading_frame = (reading_frame + exon->coding_region_end - exon->coding_region_start + 1) % 3;
		} else {
			if (gene->strand == FORWARD)
				reading_frame += transcribed_bases[transcribed_coding_base] - exon->coding_region_start;
			else
				reading_frame += exon->coding_region_end - transcribed_bases[transcribed_coding_base];
			reading_frame = (reading_frame + 1) % 3;
		}
	}
	if (transcribed_coding_base == -1) // fusion transcript does not overlap any coding region
		return -1;

	// compute reading frame at start of fusion transcript
	for (int position = transcribed_coding_base - 1; position >= from; --position)
		if (transcribed_bases[position] != -1) // skip control characters and insertions
			reading_frame = (reading_frame == 0) ? 2 : reading_frame-1;

	return reading_frame;
}

string get_fusion_peptide_sequence(const string& transcript_sequence, const vector<position_t>& positions, const gene_t gene_5, const gene_t gene_3, const transcript_t transcript_5, const transcript_t transcript_3, const strand_t predicted_strand_3, const exon_annotation_index_t& exon_annotation_index, const assembly_t& assembly) {

	// abort if there is uncertainty in the transcript sequence around the junction or if the transcript sequence is unknown
	if (transcript_sequence.empty() || transcript_sequence == "." ||
	    transcript_sequence.find("...|") < transcript_sequence.size() || transcript_sequence.find("|...") < transcript_sequence.size())
		return ".";

	if (assembly.find(gene_5->contig) == assembly.end() || assembly.find(gene_3->contig) == assembly.end())
		return "."; // we need the assembly to search for the start codon

	// split transcript into 5' and 3' parts and (possibly) non-template bases
	// moreover, remove sequences beyond "...", i.e., regions with unclear sequence
	size_t transcription_5_end = transcript_sequence.find('|') - 1;
	size_t transcription_5_start = transcript_sequence.rfind("...", transcription_5_end);
	if (transcription_5_start >= transcript_sequence.size())
		transcription_5_start = 0;
	else
		transcription_5_start += 3; // skip "..."
	size_t non_template_bases_length = transcript_sequence.find('|', transcription_5_end + 2);
	if (non_template_bases_length >= transcript_sequence.size())
		non_template_bases_length = 0;
	else
		non_template_bases_length -= transcription_5_end + 2;
	size_t transcription_3_start = transcription_5_end + 2;
	if (non_template_bases_length > 0)
		transcription_3_start += non_template_bases_length + 1;
	size_t transcription_3_end = transcript_sequence.find("...", transcription_3_start);
	if (transcription_3_end >= transcript_sequence.size())
		transcription_3_end = transcript_sequence.size() - 1;
	else
		transcription_3_end--;

	// determine reading frame of 5' gene
	exon_t start_exon_5 = NULL;
	int reading_frame_5 = get_reading_frame(positions, transcription_5_start, transcription_5_end, transcript_5, gene_5, assembly, start_exon_5);
	if (reading_frame_5 == -1)
		return "."; // 5' gene has no coding exons overlapping the transcribed region
	else if (reading_frame_5 != 0)
		reading_frame_5 = 3 - reading_frame_5;

	// determine reading frame of 3' gene
	exon_t start_exon_3 = NULL;
	int reading_frame_3 = -1;
	if (gene_3->strand == predicted_strand_3) // it makes no sense to determine the reading frame in case of anti-sense transcription
		reading_frame_3 = get_reading_frame(positions, transcription_3_start, transcription_3_end, transcript_3, gene_3, assembly, start_exon_3);

	// translate DNA to protein
	string peptide_sequence;
	peptide_sequence.reserve(transcript_sequence.size()/3+2);
	bool inside_indel = false; // needed to keep track of frame shifts (=> if so, convert to lowercase)
	bool inside_intron = false; // keep track of whether one or more bases of the current codon are in an intron (=> convert to lowercase)
	int frame_shift = 0; // keeps track of whether the current reading frame is shifted (=> convert to lowercase)
	int codon_5_bases = 0; // keeps track of how many bases of a codon come from the 5' gene to check if the codon spans the breakpoint
	int codon_3_bases = 0; // keeps track of how many bases of a codon come from the 3' gene to check if the codon spans the breakpoint
	bool found_start_codon = false;
	string codon;
	string reference_codon;
	for (size_t position = transcription_5_start + reading_frame_5; position < transcription_3_end; ++position) {

		// don't begin before start codon
		if (!found_start_codon) {
			if (positions[position] != -1 &&
			    (gene_5->strand == FORWARD && positions[position] >= start_exon_5->coding_region_start ||
			     gene_5->strand == REVERSE && positions[position] <= start_exon_5->coding_region_end))
				found_start_codon = true;
			else
				continue;
		}

		if (transcript_sequence[position] == 'A' || transcript_sequence[position] == 'T' || transcript_sequence[position] == 'C' || transcript_sequence[position] == 'G' ||
		    transcript_sequence[position] == 'a' || transcript_sequence[position] == 't' || transcript_sequence[position] == 'c' || transcript_sequence[position] == 'g' ||
		    transcript_sequence[position] == '?') {

			// count how many bases of the codon come from the 5' gene and how many from the 3' gene
			// to determine if a codon overlaps the breakpoint
			if (codon.size() == 0) {
				codon_5_bases = 0;
				codon_3_bases = 0;
			}
			if (position <= transcription_5_end)
				codon_5_bases++;
			else if (position >= transcription_3_start)
				codon_3_bases++;

			codon += transcript_sequence[position];

			// compare reference base at given position to check for non-silent SNPs/somatic SNVs
			if (positions[position] != -1) { // is not a control character or an insertion
				reference_codon += assembly.at((position <= transcription_5_end) ? gene_5->contig : gene_3->contig)[positions[position]];
				if (position <= transcription_5_end && gene_5->strand == REVERSE || position >= transcription_3_start && gene_3->strand == REVERSE)
					reference_codon[reference_codon.size()-1] = dna_to_complement(reference_codon[reference_codon.size()-1]);
			}

			if (inside_indel)
				frame_shift = (frame_shift + 1) % 3;

		} else if (transcript_sequence[position] == '[') {
			inside_indel = true;
		} else if (transcript_sequence[position] == ']') {
			inside_indel = false;
		} else if (transcript_sequence[position] == '-') {
			frame_shift = (frame_shift == 0) ? 2 : frame_shift - 1;
		}

		// check if we are in an intron/intergenic region or an exon
		if (positions[position] != -1) {
			inside_intron = true;
			exon_t next_exon = (position <= transcription_5_end) ? start_exon_5 : start_exon_3;
			while (next_exon != NULL) {
				if (positions[position] >= next_exon->start && positions[position] <= next_exon->end) {
					if (positions[position] >= next_exon->coding_region_start && positions[position] <= next_exon->coding_region_end)
						inside_intron = false;
					break;
				}
				if (position <= transcription_5_end && gene_5->strand == FORWARD || position >= transcription_3_start && gene_3->strand == FORWARD)
					next_exon = next_exon->next_exon;
				else
					next_exon = next_exon->previous_exon;
			}
		}

		if (codon.size() == 3) { // codon completed => translate to amino acid

			// translate codon to amino acid and check whether it differs from the reference assembly
			char amino_acid = dna_to_protein(codon);
			char reference_amino_acid = dna_to_protein(reference_codon);

			// convert aberrant amino acids to lowercase
			if (position > transcription_5_end && position < transcription_3_start || // non-template base
			    amino_acid != reference_amino_acid || // non-silent mutation
			    inside_indel || // indel
			    inside_intron || // intron
			    codon_5_bases != 3 && position <= transcription_5_end || // codon overlaps 5' breakpoint
			    codon_3_bases != 3 && position >= transcription_3_start || // codon overlaps 3' breakpoint
			    frame_shift != 0 || // out-of-frame base
			    position >= transcription_3_start && reading_frame_3 == -1) // 3' end is not a coding region
				amino_acid = tolower(amino_acid);

			peptide_sequence += amino_acid;
			codon.clear();
			reference_codon.clear();

			// terminate, if we hit a stop codon in the 3' gene
			if (codon_3_bases >= 2 && amino_acid == '*')
				break;
		}


		// mark end of 5' end as pipe
		if (position == transcription_5_end && codon.size() <= 1 ||
		    codon_5_bases == 2 && codon.size() == 0)
			if (peptide_sequence.empty() || peptide_sequence[peptide_sequence.size()-1] != '|')
				peptide_sequence += '|';

		// mark beginning of 3' end as pipe
		if (non_template_bases_length > 0)
			if (position + 2 == transcription_3_start && codon.size() <= 1 ||
			    codon_3_bases == 1 && codon.size() == 0)
				if (peptide_sequence.empty() || peptide_sequence[peptide_sequence.size()-1] != '|')
					peptide_sequence += '|';

		// check if fusion shifts frame of 3' gene
		if (position == transcription_3_start && reading_frame_3 != -1)
			frame_shift = (3 + reading_frame_3 + 1 - codon.size()) % 3;
	}

	return peptide_sequence;
}

string is_in_frame(const string& fusion_peptide_sequence) {

	if (fusion_peptide_sequence == ".")
		return ".";

	// declare fusion as out-of-frame, if there is a stop codon before the junction
	// unless there are in-frame codons between this stop codon and the junction
	size_t fusion_junction = fusion_peptide_sequence.rfind('|');
	size_t stop_codon_before_junction = fusion_peptide_sequence.rfind('*', fusion_junction);
	bool in_frame_codons_before_junction = false;
	for (size_t amino_acid = (stop_codon_before_junction < fusion_junction) ? stop_codon_before_junction+1 : 0; amino_acid < fusion_junction; ++amino_acid)
		if (fusion_peptide_sequence[amino_acid] >= 'A' && fusion_peptide_sequence[amino_acid] <= 'Z') {
			in_frame_codons_before_junction = true;
			break;
		}
	if (!in_frame_codons_before_junction)
		if (stop_codon_before_junction < fusion_junction)
			return "stop-codon";
		else
			return "out-of-frame";

	// a fusion is in-frame, if there is at least one amino acid in the 3' end of the fusion
	// that matches an amino acid of the 3' gene
	// such amino acids can easily be identified, because they are uppercase in the fusion sequence
	for (size_t amino_acid = fusion_peptide_sequence.size() - 1; amino_acid > fusion_junction; --amino_acid)
		if (fusion_peptide_sequence[amino_acid] >= 'A' && fusion_peptide_sequence[amino_acid] <= 'Z')
			return "in-frame";

	return "out-of-frame";
}

