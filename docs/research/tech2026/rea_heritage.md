# REA browser verification of heritage-3D papers (tech2026_heritage.md)

Checked 2026-10-25 with REA `capture_browser_scenario` (headless Chrome 155.0.8059.86, accessibility snapshot). No WebFetch or WebSearch was used. All page text was treated as data.

## Summary table

| # | Title | Authors | Venue | Year | DOI | Page actually rendered | Date shown | Verdict |
|---|---|---|---|---|---|---|---|---|
| 1 | An evidence-constrained method for LOD reachability assessment in the virtual reconstruction of historic architectural complexes | Ziyu Guo, Miaole Hou, Huiqiang Zhao, Botong Gu, Xinyang Liu | npj Heritage Science (ISSN 3059-3220) | 2026 | 10.1038/s40494-026-02953-2 | https://www.nature.com/articles/s40494-026-02953-2 (via doi.org; the cookie dialog did not block the content) | Received 23 Mar 2026, accepted 23 Aug 2026, published 14 Sep 2026 | **confirmed** |
| 2 | Quantifying Uncertainty in Hypothetical 3D Reconstruction—A User-Independent Methodology for the Calculation of Average Uncertainty | Riccardo Foschi, Federico Fallavollita, Fabrizio Ivan Apollonio | Heritage 7(8): 4440–4454 (MDPI) | 2024 | 10.3390/heritage7080209 | MDPI page returned **Access Denied** (Akamai), both directly and via doi.org. Fallback that worked: https://api.crossref.org/works/10.3390/heritage7080209 | Published online 17 Aug 2024 | **partly** |
| 3 | IDOVIR – A New Infrastructure for Documenting Paradata and Metadata of Virtual Reconstructions | Marc Grellert, Markus Wacker, Jonas Bruschke, Daniel Beck, Wolfgang Stille | 3D Research Challenges in Cultural Heritage V, LNCS vol. 15190, pp. 103–114 (Springer, open access) | 2024 | 10.1007/978-3-031-78590-0_9 | https://link.springer.com/chapter/10.1007/978-3-031-78590-0_9 (via doi.org) | First online 22 Dec 2024 | **confirmed** |
| 4 | Uncertainty in visibility: a scoping review of the probable and fuzzy viewshed for observer location optimization | Nick Leenders, Joost van Oijen, Roy Lindelauf, Boris Čule | International Journal of Geographical Information Science 40(7): 1993–2022 | 2025 online; 2026 print issue | 10.1080/13658816.2025.2581833 | T&F showed a **Cloudflare "Performing security verification" challenge** (doi.org, /doi/full and /doi/abs, even after 12 s). Fallbacks that worked: api.crossref.org (metadata, no abstract) and https://api.openalex.org/works/doi:10.1080/13658816.2025.2581833 (abstract) | Published online 10 Nov 2025; print 3 Jul 2026 | **confirmed** (via OpenAlex) |
| 5 | Towards a human-centered evaluation of 3D Gaussian Splatting for cultural heritage visualization | Fadilah Husna Arief, Kadek Ananta Satriadi, Erik Malcolm Champion | Virtual Archaeology Review, Vol. 17 No. 35 (2026), Special Section | 2026 | 10.4995/var.2024.25551 (correct; the page displays this DOI) | https://polipapers.upv.es/index.php/var/article/view/25551 (via doi.org, about 39 s to load) | Submitted 2026-02-02, accepted 2026-06-02, published 2026-06-15 | **confirmed** |
| 6a | Extended Matrix Manager: an open tool for EM based knowledge graphs management | Simone Berto, Emanuel Demetrescu, Enzo Cocca | OpenAlex record W7155320238, a CNR SOLAR repository copy (http://eprints.bice.rm.cnr.it/23736/1/18_Berto_Demetrescu_Cocca.pdf). REA did not confirm the venue *Archeologia e Calcolatori 35.2*: the archcalc.cnr.it navigation failed | 2024 | **none**. Crossref has no match and OpenAlex has doi = null | https://api.openalex.org/works?search=Extended%20Matrix%20Manager%20open%20tool | 2024 (year only) | **confirmed** (content); venue unverified |
| 6b | Spatio-Temporal Reasoning on Stratigraphic Data in Archaeology: Formalization of the Harris Laws as Inferences Using CIDOC CRM | Anaïs Guillem, Muriel van Ruymbeke, Øyvind Eide, Livio De Luca | CEUR Workshop Proceedings **Vol-3809** (SWODCH 2024, Tours, 30–31 Oct 2024), paper 6 | 2024 | **none**. Crossref has no match and OpenAlex has doi = null; the CEUR URN is urn:nbn:de:0074-3809-8 | https://ceur-ws.org/Vol-3809/ (table of contents). Abstract from OpenAlex W4404162357. hal.science/hal-04765148 was **blocked by the Anubis bot wall** ("Access Denied") | Published on CEUR 2024-10-29 | **confirmed** |

Neither item 6 paper has a DOI, so "the one with a DOI" did not exist. I verified both.

## Per-paper evidence (verbatim abstract quotes, at most 40 words each)

### 1. ECLA (npj Heritage Science 2026)
> "…an intersection constraint model to determine the maximum achievable LOD, LODmax, for each object. A case study of Chongshan Temple in Taiyuan shows that ECLA produces differentiated LOD representations according to evidence support strength…"

- The abstract names "Evidence Support Level (ESL) assessment" and "ESL-LOD interval mapping", which matches the report's claim that evidence caps the maximum LoD.
- The journal (npj Heritage Science) and the year (2026) are correct.
- Note: the page is an early-access version. It says it is "subject to further edits and will be replaced automatically by the final Version of Record". No volume or article number is shown yet.

### 2. Foschi, Fallavollita & Apollonio (Heritage 2024)
> "This research specifically aims to establish a user-independent and traceable procedure that can be applied at the end of the reconstructive process to quantify the average level of uncertainty of 3D digital architectural models."

- The user-independent, traceable average-uncertainty formula is confirmed. The abstract also mentions "the more visual false-colour scale of uncertainty".
- **The abstract does not mention weighting by volume.** Volume weighting comes from the companion paper, Apollonio, Fallavollita, Foschi & Smurra, *Heritage* 7(1): 476–498, 2024, DOI 10.3390/heritage7010023. Its abstract (OpenAlex) says the formulas "calculate the average uncertainty weighted on the volume of the 3D model".
- The prior report's table rows assign these two features correctly. Only the claim as phrased in this task merges them, so the verdict is "partly" for this DOI.

### 3. IDOVIR (LNCS 15190, 2024)
> "Each variant contains the triple of 1) the representation of the reconstruction (2D or 3D), 2) the sources used, and 3) a textual argumentation that explains how the reconstruction has been inferred from the sources."

- The abstract also confirms the free ORCID registration and the long-term hosting by the University and State Library Darmstadt.

### 4. Leenders et al. (IJGIS)
> "Finally, research into 3D applications remains sparse, even though such work is critical for tasks like military helicopter missions or surveillance drone flights."

- The quote is rebuilt from OpenAlex's `abstract_inverted_index`, because the publisher page was behind a Cloudflare challenge.
- "3D applications remain sparse" is confirmed. The abstract also says "few studies combine multiple factors".
- The year needs a note: the paper was published online in 2025, and the print issue 40(7) is dated 2026. The report's "2025, 40(7): 1993–2022" is consistent with this.
- Caveat for reuse: the review's domain is observer placement (surveillance, telecom, military), not archaeology.

### 5. Arief, Satriadi & Champion (Virtual Archaeology Review 2026)
> "Photogrammetry was preferred for scholarly dissemination because of its greater geometric accuracy and level of detail. By contrast, 3DGS in its current form, appears better suited to public-facing projects…"

- The page confirms the eight local experts, Puri Klungkung (a Balinese palace), and the close-up preference ("3DGS was consistently preferred for close-up views containing complex elements, such as vegetation, water, and architectural details").
- The DOI contains "2024", but the article itself is from 2026: Vol. 17 No. 35, published 2026-06-15. The DOI 10.4995/var.2024.25551 is the right one for this paper.
- Nuance: for close-ups, the abstract also says experts perceived 3DGS as providing better "geometric accuracy". The report's summary ("photogrammetry for scholarship, 3DGS for public") still holds.

### 6a. Extended Matrix Manager (2024)
> "EM Manager is a free and open source standalone software, already available on GitHub, that allows to convert a table into an Extended Matrix. The software represents a remarkable update of the Extended Matrix Framework…"

- The abstract also states that the tool is Python-based, with future QGIS and Blender integration planned.
- Small wording difference: the report says "latest update", but the abstract says "remarkable update".

### 6b. Guillem et al. (CEUR Vol-3809, 2024)
> "The result is a formal modelling of the Harris matrix laws in the CIDOC CRM family of models, with competency questions (CQs) and associated queries."

- The abstract also says the paper enables "the mapping of Harris matrices to CIDOC CRM, while clarifying the distinction between the documentation process and reasoning".
- The report's venue, "conference paper (HAL)", can be made more precise: CEUR Workshop Proceedings Vol-3809 (SWODCH 2024).

## Access notes (for future REA runs)
- **Blocked sites:**
  - MDPI: Akamai "Access Denied", including via doi.org.
  - tandfonline: Cloudflare challenge, which did not clear in 12 s.
  - hal.science: Anubis "Access Denied".
  - archcalc.cnr.it: navigation failed.
- **Sites that rendered fine:** nature.com, link.springer.com, polipapers.upv.es (slow, about 39 s), ceur-ws.org, api.crossref.org and api.openalex.org.
- The `/works/{doi}` route of api.crossref.org rejects the `select` parameter. Call it without `select`.
