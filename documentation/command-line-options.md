Arriba
------

**Usage**

```bash
arriba [-c Chimeric.out.sam] -x Aligned.out.sam \
       -g annotation.gtf -a assembly.fa [-b blacklists.tsv] [-k known_fusions.tsv] \
       -o fusions.tsv [-O fusions.discarded.tsv] \
       [OPTIONS]
```

**Options**

`-c FILE`
: File in SAM/BAM/CRAM format with chimeric alignments as generated by STAR (`Chimeric.out.sam`). This parameter is only required, if STAR was run with the parameter `--chimOutType SeparateSAMold`. When STAR was run with the parameter `--chimOutType WithinBAM`, it suffices to pass the parameter `-x` to Arriba and `-c` can be omitted.

`-x FILE`
: File in SAM/BAM/CRAM format with main alignments as generated by STAR (`Aligned.out.sam`). Arriba extracts candidate reads from this file.

`-g FILE`
: GTF file with gene annotation. The file may be gzip-compressed.

`-G GTF_FEATURES`
: Comma-/space-separated list of names of GTF features. The names of features in GTF files are not standardized. Different publishers use different names for the same features. For example, GENCODE uses `gene_type` for the gene type feature, whereas ENSEMBL uses `gene_biotype`. In order that Arriba can parse the GTF files from various publishers, the names of GTF features is configurable. Alternative names for one and the same feature can be specified by using the pipe symbol as a separator (`|`). Arriba supports a set of names which is suitable for RefSeq, GENCODE, and ENSEMBL. Default: `gene_name=gene_name gene_id=gene_id transcript_id=transcript_id gene_status=gene_status|gene_type|gene_biotype status_KNOWN=KNOWN|protein_coding gene_type=gene_type|gene_biotype type_protein_coding=protein_coding feature_exon=exon feature_UTR=UTR feature_gene=gene`

`-a FILE`
: FastA file with genome sequence (assembly). The file may be gzip-compressed. An index with the file extension `.fai` must exist only if CRAM data is processed.

`-b FILE`
: File containing blacklisted ranges. Refer to section [Blacklist](input-files.md#blacklist) for a description of the expected file format. The file may be gzip-compressed.

`-k FILE`
: File containing known/recurrent fusions. Some cancer entities are often characterized by fusions between the same pair of genes. In order to boost sensitivity, a list of known fusions can be supplied using this parameter. Refer to section (Known fusions)[input-files.md#known-fusions] for a description of the expected file format. The file may be gzip-compressed.

`-o FILE`
: Output file with fusions that have passed all filters. Refer to section [fusions.tsv](output-files.md#fusionstsv) for a description of the columns.

`-O FILE`
: Output file with fusions that were discarded due to filtering. The format is the same as for parameter `-o`.

`-d FILE`
: Tab-separated file with coordinates of structural variants found using whole-genome sequencing data. These coordinates serve to increase sensitivity towards weakly expressed fusions and to eliminate fusions with low confidence. Refer to section [Structural variant calls from WGS](input-files.md#structural-variant-calls-from-wgs) for a description of the expected file format. The file may be gzip-compressed.

`-D MAX_GENOMIC_BREAKPOINT_DISTANCE`
: When a file with genomic breakpoints obtained from whole-genome sequencing is supplied via the parameter `-d`, this parameter determines how far a genomic breakpoint may be away from a transcriptomic breakpoint to still consider it as a related event. For events inside genes, the distance is added to the end of the gene; for intergenic events, the distance threshold is applied as is. Default: `100000`

`-s STRANDEDNESS`
: Whether a strand-specific protocol was used for library preparation, and if so, the type of strandedness:

- `auto`: auto-detect whether the library is stranded and the type of strandedness

- `yes`: the library is stranded and the strand of the read designated as first-in-pair matches the transcribed strand

- `no`: the library is not stranded

- `reverse`: the library is stranded and the strand of the read designated as first-in-pair is the reverse of the transcribed strand

Even when an unstranded library is processed, Arriba can often infer the strand from splice-patterns. But in unclear situations, stranded data helps resolve ambiguities. Default: `auto` 

`-i CONTIGS`
: Comma-/space-separated list of interesting contigs. Fusions between genes on other contigs are ignored. Contigs can be specified with or without the prefix `chr`. Default: `1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y`

`-f FILTERS`
: Comma-/space-separated list of filters to disable. By default all filters are enabled. Valid values are: `uninteresting_contigs`, `non_coding_neighbors`, `merge_adjacent`, `pcr_fusions`, `spliced`, `select_best`, `hairpin`, `small_insert_size`, `genomic_support`, `read_through`, `mismatches`, `homopolymer`, `long_gap`, `many_spliced`, `isoforms`, `intronic`, `end_to_end`, `known_fusions`, `inconsistently_clipped`, `duplicates`, `blacklist`, `homologs`, `intragenic_exonic`, `relative_support`, `min_support`, `same_gene`, `mismappers`, `no_coverage`, `short_anchor`, `no_genomic_support`, `low_entropy`

`-E MAX_E-VALUE`
: Arriba estimates the number of fusions with a given number of supporting reads which one would expect to see by random chance. If the expected number of fusions (e-value) is higher than this threshold, the fusion is discarded by the filter `relative_support`. Note: Increasing this threshold can dramatically increase the number of false positives and may increase the runtime of resource-intensive steps. Fractional values are possible. Default: `0.3`

`-S MIN_SUPPORTING_READS`
: The filter `min_support` discards all fusions with fewer than this many supporting reads (split reads and discordant mates combined). Default: `2`

`-m MAX_MISMAPPERS`
: When more than this fraction of supporting reads turns out to be mapped incorrectly, the filter `mismappers` discards the fusion. Default: `0.8`

`-L MAX_HOMOLOG_IDENTITY`
: Genes with more than the given fraction of sequence identity are considered homologs and removed by the filter `homologs`. Default: `0.3`

`-H HOMOPOLYMER_LENGTH`
: The filter `homopolymer` removes breakpoints adjacent to homopolymers of the given length or more. Default: `6`

`-R READ_THROUGH_DISTANCE`
: The filter `read_through` removes read-through fusions where the breakpoints are less than the given distance away from each other. Default: `10000`

`-A MIN_ANCHOR_LENGTH`
: Alignment artifacts are often characterized by split reads coming from only one gene and no discordant mates. Moreover, the split reads only align to a short stretch in one of the genes. The filter `short_anchor` removes these fusions. This parameter sets the threshold in bp for what the filter considers short. Default: `23`

`-M MANY_SPLICED_EVENTS`
: The filter `many_spliced` recovers fusions between genes that have at least this many spliced breakpoints. Default: `4` 

`-K MAX_KMER_CONTENT`
: The filter `low_entropy` removes reads with repetitive 3-mers. If the 3-mers make up more than the given fraction of the sequence, then the read is discarded. Default: `0.6`

`-V MAX_MISMATCH_PVALUE`
: The filter `mismatches` uses a binomial model to calculate a p-value for observing a given number of mismatches in a read. If the number of mismatches is too high, the read is discarded. Default: `0.01`

`-F FRAGMENT_LENGTH`
: When paired-end data is given, the fragment length is estimated automatically and this parameter has no effect. But when single-end data is given, the mean fragment length should be specified to effectively filter fusions that arise from hairpin structures. Default: `200`

`-U MAX_READS`
: Subsample fusions with more than the given number of supporting reads. This improves performance without compromising sensitivity, as long as the threshold is high. Counting of supporting reads beyond the threshold is inaccurate, obviously. Default: `300`

`-Q QUANTILE`
: Highly expressed genes are prone to produce artifacts during library preparation. Genes with an expression above the given quantile are eligible for filtering by the filter `pcr_fusions`. Default: `0.998`

`-T`
: When set, the column `fusion_transcript` is populated with the sequence of the fused genes as assembled from the supporting reads. Specify the flag twice to also print the fusion transcripts to the file containing discarded fusions (`-O`). Refer to section [fusions.tsv](output-files.md#fusionstsv) for a description of the format of the column. Default: off

`-P`
: When set, the column `peptide_sequence` is populated with the sequence of the peptide around the fusion junction as translated from the fusion transcript. Specify the flag twice to also print the peptide sequence to the file containing discarded fusions (`-O`). Refer to section [fusions.tsv](output-files.md#fusionstsv) for a description of the format of the column. Default: off

`-I`
: When set, the column `read_identifiers` is populated with identifiers of the reads which support the fusion. The identifiers are separated by commas. Specify the flag twice to also print the read identifiers to the file containing discarded fusions (`-O`). Default: off

`-h`
: Print help and exit.

draw_fusions.R
--------------

**Usage**

```bash
draw_fusions.R --annotation=annotation.gtf --fusions=fusions.tsv --output=output.pdf \
               [--alignments=Aligned.sortedByCoord.out.bam] \
               [--cytobands=cytobands.tsv] [--proteinDomains=protein_domains.gff3] \
               [OPTIONS]
```

**Options**

`--fusions=FILE`
: Ouput file `fusions.tsv` containing the gene fusion predictions which passed all filters of Arriba.

`--annotation=FILE`
: Gene annotation in GTF format.

`--output=FILE`
: Output file in PDF format containing the visualizations of the gene fusions.

`--cytobands=FILE`
: Coordinates of the Giemsa staining bands. This information is used to draw ideograms. If the argument is omitted, then no ideograms are rendered. The file must have the following columns: `contig`, `start`, `end`, `name`, `giemsa`. Recognized values for the Giemsa staining intensity are: `gneg`, `gpos` followed by a percentage, `acen`, `stalk`. Distributions of Arriba provide Giemsa staining annotation for all supported assemblies in the `database` directory.

`--minConfidenceForCircosPlot=low|medium|high`
: Specifies the minimum confidence that a prediction must have to be included in the circos plot. It usually makes no sense to include low-confidence fusions in circos plots, because they are abundant and unreliable, and would clutter up the circos plot. Default: `medium`

`--alignments=FILE`
: BAM file containing normal alignments from STAR (`Aligned.sortedByCoord.out.bam`). The file must be sorted by coordinates and indexed. If this argument is given, the script generates coverage plots. This argument requires the Bioconductor package `GenomicAlignments`.

`--pdfWidth=INCHES`
: Width of the pages of the PDF output file in inches. Default: `11.692`

`--pdfHeight=INCHES`
: Height of the pages of the PDF output file in inches. Default: `8.267`

`--squishIntrons=TRUE|FALSE`
: Exons usually make up only a small fraction of a gene. They may be hard to see in the plot. Since introns are in most situations of no interest in the context of gene fusions, this switch can be used to shrink the size of introns to a fixed, negligible size. It makes sense to disable this feature, if breakpoints in introns are of importance. Default: `TRUE`

`--color1=COLOR`
: Color of the 5' end of the fusion. The color can be specified in any notation that is a valid color specification in R. Default: `#e5a5a5`

`--color2=COLOR`
: Color of the 3' end of the fusion. The color can be specified in any notation that is a valid color specification in R. Default: `#a7c4e5`

`--printExonLabels=TRUE|FALSE`
: By default the number of an exon is printed inside each exon, which is taken from the attribute `exon_number` of the GTF annotation. When a gene has many exons, the boxes may be too narrow to contain the labels, resulting in unreadable exon labels. In these situations, it may be better to turn off exon labels. Default: `TRUE`

`--proteinDomains=FILE`
: GFF3 file containing the genomic coordinates of protein domains. Distributions of Arriba offer protein domain annotations for all supported assemblies in the `database` directory. When this file is given, a plot is generated, which shows the protein domains retained in the fusion transcript. This option requires the Bioconductor package `GenomicRanges`.

`--mergeDomainsOverlappingBy=FRACTION`
: Occasionally, domains are annotated redundantly. For example, tyrosine kinase domains are frequently annotated as `Protein tyrosine kinase` and `Protein kinase domain`. In order to simplify the visualization, such domains can be merged into one, given that they overlap by the given fraction. The description of the larger domain is used. Default: `0.9`

`--optimizeDomainColors=TRUE|FALSE`
: By default, the script colorizes domains according to the colors specified in the file given in `--annotation`. This way, coloring of domains is consistent across all proteins. But since there are more distinct domains than colors, this can lead to different domains having the same color. If this option is set to `TRUE`, the colors are recomputed for each fusion separately. This ensures that the colors have the maximum distance for each individual fusion, but they are no longer consistent across different fusions. Default: `FALSE`

`--fontSize=SIZE`
: Decimal value to scale the size of text. Default: `1`

`--showIntergenicVicinity=DISTANCE`
: This option only applies to intergenic breakpoints. If it is set to a value greater than 0, then the script draws the genes which are no more than the given distance away from an intergenic breakpoint. Note that this option is incompatible with `--squishIntrons`. Default: `0`
