# 3D methods for archaeology and cultural heritage, 2024–2026: research scan for Strata 3D

Compiled 2026-10-25. The sources cover 2024 to 2026, with a few pre-2024 baselines that are labelled as baselines.

## How sources were verified

- **yes (page)**: I opened the publisher abstract page with WebFetch.
- **yes (abstract)**: I read the abstract in the Crossref or OpenAlex record for that DOI. The bibliographic data comes from the registry, and I read the abstract text.
- **metadata only**: Crossref or OpenAlex confirms that the paper exists, with its title, authors, venue and DOI, but gives no abstract. For these papers, the tables state only what the title says, never their results.
- Anything I saw only in a search-engine snippet is listed under **Unverified** at the end and is not cited in the tables.
- MDPI, Taylor & Francis, ScienceDirect and polipapers (VAR) pages returned 403 or TLS errors, so their abstracts were taken from the Crossref or OpenAlex records.
- I read abstracts only, never full texts. Where a quantitative result is given, it is quoted from the abstract.

---

## 1. Placing trial-trench features in 3D by depth (top and bottom elevation)

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| A review position paper. Excavations now bring data into 3D successfully, but 3D GIS is used mainly for visualization and information management, and still rarely for stratigraphic analysis or interpretation. | 3D GIS in archaeological excavations: linking documentation with analytic and synthetic workflows | M. Katsianis | Journal of Greek Archaeology 9: 65–82 | 2024 | 10.32028/jga.v9i.2736 | peer-reviewed | yes (abstract) |
| A "true 3D GIS" for the Keros/Dhaskalio excavations. It holds more than 1,000 SfM models, offers multi-layered 3D views, and links all geolocated excavation and specialist data in one platform. | A holistic solution for the analysis of excavation and specialist data in a 3D GIS framework | R. Campbell, M. J. Boyd, J. Herbst, H. R. Indgjerd, N. Meyer, C. Renfrew | Journal of Greek Archaeology 9: 47–64 | 2024 | 10.32028/jga.v9i.2735 | peer-reviewed | yes (abstract) |
| Combines legacy section drawings with georeferenced photogrammetric models of recent sondages in a 3D GIS. This makes it possible to correct elevation data, reconstruct strata that no longer survive, and interpolate missing contexts (Hala Sultan Tekke). | Old Data, New Horizons: 3D Modeling as a Catalyst for Recontextualizing Fragmentary Legacy Data | P. De Weirdt, K. Nys, M. Recke | Advances in Archaeological Practice | 2026 | 10.1017/aap.2025.10134 | peer-reviewed | yes (abstract) |
| Field recording in QField with an external DGNSS receiver over Bluetooth, which gives the accuracy that stratigraphic work needs. Each stratigraphic unit is uniquely encoded at the moment it is captured. | Let's Dig with QField! A F/OSS Mobile GIS Management System for Archaeological Excavation | M. Rossi | Advances in Archaeological Practice | 2026 | 10.1017/aap.2025.10143 | peer-reviewed | yes (abstract) |
| A five-level spatio-temporal data model (site, square unit, layer, feature, cultural period) for excavation reports, with support for spatial-relationship queries. | A data model for the spatialized integration of archaeological excavation information from prehistoric sites | T. Hou, Y. Li, D. Hu, J. Shi, G. Lü | npj Heritage Science | 2026 | 10.1038/s40494-026-02316-x | peer-reviewed | yes (abstract) |
| Sequential SfM models of a pot-burial excavation shown in an interactive app. A user evaluation with professional and general audiences rated it intuitive and effective. | Spatiotemporal reconstruction of pot burial excavations | M. K. Grigoriadi, G. Papaioannou | CAA Proceedings 52(1) | 2026 | 10.64888/caaproceedings.v52i1.1179 | conference (peer-reviewed proceedings) | yes (abstract) |
| SfM models used in excavation and standing-remains assessment at the Stabian Baths in Pompeii, both as analytical tools and for publication. The paper also discusses the problems of handling very large datasets. | The Use and Application of SfM-Based Documentation in Excavation and Standing Remains Assessment of the Stabian Baths, Pompeii | C. Brünenberg, C. Rummel, M. Trümper | In: The 3 Dimensions of Digitalised Archaeology (Springer) | 2024 | 10.1007/978-3-031-53032-6_4 | book chapter | yes (abstract) |

**Implication for Strata 3D**
- Store each trench feature as a **depth interval**: `z_top` and `z_bottom` in absolute elevation, together with the datum and how it was measured (total station, DGNSS, or estimated from a drawing). Render the feature as an extruded prism or volume between the two values, not as a flat polygon draped on the terrain. Katsianis' criticism is that 3D GIS stays "only visualization". The answer is to make depth queryable, for example "which features intersect the paleo-surface at 34 m a.s.l.?".
- Following De Weirdt et al., let legacy elevations be corrected against a reference model, and keep both the original and the corrected value. A corrected elevation is tier B. A value interpolated for a missing context is tier C.
- Adopt a single SU/feature identity, as in QField and the Hou et al. hierarchy, so that the Strata 2D survey data and the 3D view share the same keys. In Korean trial-trench (시굴) practice the hierarchy maps to trench, then layer, then feature (유구).
- Grigoriadi and Papaioannou support showing **excavation sequence as time steps**. This could be a slider that removes layers in order of excavation.

## 2. Reconstruction alternatives A/B and tier rendering

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| IDOVIR: each entity is split into spatial areas and time periods, and **several variants** can be assigned to each. Every variant is a triple: the representation (2D or 3D), the sources used, and a written argument. The service is free with ORCID login and hosted by ULB Darmstadt. | IDOVIR – A New Infrastructure for Documenting Paradata and Metadata of Virtual Reconstructions | M. Grellert, M. Wacker, J. Bruschke, D. Beck, W. Stille | LNCS (3D Research Challenges in CH V) | 2024 | 10.1007/978-3-031-78590-0_9 | conference/book chapter | yes (abstract) |
| A CoVHer documentation workflow that extends the Scientific Reference Model (SRM) and the Critical Digital Model (CDM). It documents all decisions and inferences and uses open formats. | Documentation and Publication of Hypothetical Virtual 3D Reconstructions in the CoVHer Project | I. P. Bajena, F. I. Apollonio, K. Argasiński, F. Fallavollita, R. Foschi, J. Franczuk, K. Koszewski, P. Kuroczyński, J. Lutteroth | LNCS | 2024 | 10.1007/978-3-031-78590-0_10 | conference/book chapter | yes (abstract) |
| CoVHer project standards for 3D digital reconstructions (2022–2025 Erasmus+ project). | Reviving Europe's Architectural Heritage: The CoVHer Project's Standards for 3D Digital Reconstructions | F. I. Apollonio, F. Fallavollita, R. Foschi | LNCS (3D Research Challenges in CH VI): 112–124 | 2025 | 10.1007/978-3-032-05656-6_11 | conference/book chapter | no (metadata only) |
| The SRM concept applied in a BIM-compliant reconstruction of the Ashmyany synagogue. Elements approximated for lack of sources are flagged. | Digital 3D reconstructions of synagogues for an innovative approach on Jewish architectural heritage in East Central Europe | J. Lutteroth, P. Kuroczyński, I. P. Bajena | Virtual Archaeology Review 16(32): 144–160 | 2025 | 10.4995/var.2024.22542 | peer-reviewed | yes (abstract) |
| Versioning reconstruction hypotheses and **counterfactual trajectories**, using reasoning plus 2D and 3D visualization (Notre-Dame voussoirs). | Versioning Virtual Reconstruction Hypotheses: Revealing Counterfactual Trajectories of the Fallen Voussoirs of Notre-Dame de Paris Using Reasoning and 2D/3D Visualization | A. Guillem, J. Samuel, G. Gesquière, L. De Luca, V. Abergel | LNCS | 2025 | 10.1007/978-3-031-78955-7_18 | conference/book chapter | no (metadata only) |
| **ECLA (Evidence-Constrained LoD Assessment)**: maps an Evidence Support Level (ESL) to geometric LoD, so the available evidence caps the maximum reconstruction detail (LoDmax) for each object (Chongshan Temple, Taiyuan). | An evidence-constrained method for LOD reachability assessment in the virtual reconstruction of historic architectural complexes | Z. Guo, M. Hou, H. Zhao, B. Gu, X. Liu | npj Heritage Science | 2026 | 10.1038/s40494-026-02953-2 | peer-reviewed | yes (abstract) |
| **Level of Reference (LoRef)**: every HBIM element is linked to its supporting sources using the IDOVIR Source Classification Taxonomy mapped to bSDD. The model holds several historical configurations of the Priene Theatre. Reliability is derived from source typology rather than set as a predefined metric. | From sources to Levels of Reference (LoRef) for the virtual reconstructions of the Priene Theatre: an interoperable and informative HBIM workflow | E. C. Giovannini, J. Bono | ISPRS Archives XLVIII-2/W12-2026: 167 | 2026 | 10.5194/isprs-archives-xlviii-2-w12-2026-167-2026 | conference (ISPRS) | yes (abstract) |
| EM-BIM: Extended Matrix paradata carried into HBIM through visual programming and a bSDD mapping to IFC classes (Doric Stoa, Priene). | EM-BIM: VPL interoperable processes for paradata management in archaeological virtual reconstructions using Extended Matrix (EM) and bSDD as knowledge representation systems | A. Sanseverino, E. C. Giovannini | ISPRS Archives XLVIII-2/W12-2026: 423–430 | 2026 | 10.5194/isprs-archives-xlviii-2-w12-2026-423-2026 | conference (ISPRS) | yes (abstract) |
| EM Manager: an open-source Python standalone tool that converts a table into an Extended Matrix graph. The authors describe it as the latest update of the EM framework. | Extended Matrix Manager: an open tool for EM based knowledge graphs management | S. Berto, E. Demetrescu, E. Cocca | Archeologia e Calcolatori 35.2 (venue inferred from archcalc.cnr.it URL; OpenAlex lists CNR SOLAR) | 2024 | https://www.archcalc.cnr.it/indice/PDF35.2/18_Berto_Demetrescu_Cocca.pdf | peer-reviewed | yes (abstract via OpenAlex; no DOI) |
| An XR/VR science-communication framework derived from the London Charter and the Seville Principles. It covers storytelling, agency and interactivity. | Standards for science communication in extended and virtual reality: a model for XR/VR based on London Charter and Seville Principles | J. L. Rubio Tamayo, D. L. Wuebben, M. Gertrudix | Journal of Science Communication 23(03) | 2024 | 10.22323/2.23030203 | peer-reviewed | yes (abstract) |
| A critical review of 3D virtual reconstruction and the charters, with guidelines and the ethics of "misleading and fake messages". The abstract dates the Seville Principles to 2011. | Insight on 3D Virtual Reconstruction of Architectural and Archaeological Cultural Heritage: Critical Assessment, Comprehensive Guidelines, and Recommendations | N. A. Haddad | Studies in Conservation 71(6): 511–530 | 2025 | 10.1080/00393630.2025.2529089 | peer-reviewed | yes (abstract) |

**What supersedes the older baselines.** The London Charter (2009) and the Seville Principles set out principles of transparency and intellectual integrity. The 2024–2026 work turns those principles into data structures:
- variant triples of representation, sources and argument (IDOVIR);
- source-linked levels per element (LoRef);
- evidence-capped LoD (ECLA);
- graph paradata (Extended Matrix, EM Manager, EM-BIM);
- the SRM/CDM documentation standards (CoVHer).

**Implication for Strata 3D**
- Model each reconstructable object as `{object_id, variants[]}`, where each **variant = (geometry, sources[], argument text, author, date, tier)**, following IDOVIR. Hypotheses A and B are simply two variants. Never overwrite one with the other.
- Make **tier = f(sources)** rather than a free choice, in the spirit of LoRef and ECLA. Tier A requires measured survey data, B a documented source such as an excavation report or drawing, C an inference by analogy, and D is a schematic placeholder. Also cap the geometric detail by tier, as ECLA does. A tier-C building should never be rendered at roof-tile LoD. Use massing or simplified geometry, so that rendering detail does not imply more certainty than the evidence supports.
- Provide an A/B toggle and a side-by-side or swipe compare view. The on-screen label shows the variant's argument and sources, which is the paradata.
- Export the variant and paradata graph as Extended-Matrix-compatible CSV or GraphML, or in an IDOVIR-compatible structure. This gives interoperability with the European tool ecosystem.

## 3. Paleo ground surface DEM

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| Voxel models of the urban subsurface (medieval Odense) built from legacy and recent archaeological and geological data. They outperform 2D and pseudo-3D models for the volume and preservation state of cultural layers, and show modern construction reducing preserved deposits. | Predicting Buried Archaeological Heritage in a 3D Environment—Modelling the Urban Subsurface Using Voxels | K. Haase, T. M. Pallesen, S. M. Sindbæk, S. M. Kristiansen | Geoarchaeology 40(3) | 2025 | 10.1002/gea.70014 | peer-reviewed | yes (abstract) |
| A multi-temporal 3D geomodel combining historical cadastral maps, boreholes and dynamic probing. It is validated by MASW and HVSR geophysics, and reveals buried valleys and changing backfill thickness (Civita Castellana). | Reconstructing urban landscape evolution with historical maps and geophysical techniques: 3D time-sensitive and multi-source geomodelling | P. Ciampi, L. M. Giannini, C. Esposito, S. Younsi | Environmental Earth Sciences 84(9) | 2025 | 10.1007/s12665-025-12260-x | peer-reviewed | yes (abstract) |
| An open-data release of diachronic **palaeoDEMs** and palaeogeographic reconstructions for the Ravenna hinterland, intended for development-led desktop assessment. | Geoarchaeology and PalaeoDEMs Modelling to Assess the Archaeological Potential of Ravenna and Its Hinterland | M. Abballe | Journal of Open Archaeology Data | 2025 | 10.5334/joad.148 | peer-reviewed data paper | yes (abstract) |
| *Baseline (pre-2024)*: a paleotopography of Aachen from about 1,437 excavation and drilling points using kriging, plus a map of anthropogenic deposit thickness. | Paleotopography and anthropogenic deposition thickness of the city of Aachen, Germany | (authors not verified) | Journal of Maps | 2019 | 10.1080/17445647.2019.1590248 | peer-reviewed | no (snippet only; see Unverified) |

**Implication for Strata 3D**
- Build the paleo-surface as **modern DEM minus interpolated overburden thickness**. The control points are the trench layer-interface elevations from feature 1, plus boreholes and legacy reports. Kriging is the established interpolator in the Aachen baseline (snippet only). A search snippet also attributes kriging to Ciampi et al., but their abstract does not state it. Show the kriging variance or standard error as a second raster. Without that raster the paleo-DEM looks more certain than it is.
- The paleo-DEM is inherently tier C, or tier B close to dense control points. Render it with tier styling and show the control points as pins, so the audience sees where the data really is.
- Haase et al. argue for voxels when the question is about deposit volume or preservation. For Strata's talk use-case, a 2.5D paleo-surface plus extruded trench volumes is enough. Treat voxels as optional (see section 5).
- Support several period surfaces, as the Ravenna palaeoDEMs do. Examples are a Silla-period surface and the modern surface. Let the user flip between them with a disclosed source list.

## 4. Viewshed using reconstructed building heights

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| A scoping review of **probable and fuzzy viewsheds**. Binary visibility ignores DEM uncertainty, vegetation, haze and target size. Few studies combine several factors, and **3D applications remain sparse**. | Uncertainty in visibility: a scoping review of the probable and fuzzy viewshed for observer location optimization | N. Leenders, J. van Oijen, R. Lindelauf, B. Čule | International Journal of Geographical Information Science 40(7): 1993–2022 | 2025 | 10.1080/13658816.2025.2581833 | peer-reviewed (review) | yes (abstract via OpenAlex) |
| Intervisibility and prominence of more than 1,000 burial mounds (Bulgaria). It uses **vegetation simulation and Monte Carlo modelling** to get past the bare-earth DEM, edge effects and small study areas. | Assessing Burial Mound Intervisibility and Prominence at Regional Scale | A. Sobotkova | conference paper (OpenAlex record; venue not given) | 2024 | (no DOI; OpenAlex record) | conference | yes (abstract via OpenAlex) |
| Four visibility methods, including a bespoke hybrid method, applied to the **built environment** of an Iron Age site. The two largest houses fall outside the dense inter-visible monitoring of the rest of the site. | Visual affordances and social interactions at Early Iron Age Khirbat al-Mudayna al-ʽAliya, Jordan | B. E. Routledge, A. Wilson | Levant | 2024 | 10.1080/00758914.2024.2386191 | peer-reviewed | yes (abstract) |
| A normalized cumulative viewshed of 102 nuraghi with model comparison. It finds **no** visual-control structure, which is a useful example of a negative test against random expectation. | Settlement Organization and Distribution in Bronze Age Sardinia: Utilizing Cumulative Viewshed Analysis and Spatial Statistics in the Sulcis Plain | M. Rothenberg, T. P. Leppard, E. Weitzel, E. A. Murphy | Journal of Field Archaeology 49(5): 356–370 | 2024 | 10.1080/00934690.2024.2318682 | peer-reviewed | yes (abstract via OpenAlex) |
| Line-of-sight on a LiDAR bare-earth raster, with sites compared to **random points**. Sites were more visible than random, and most were within view of a main mound group (Greater Cahokia). | Constructing Greater Cahokia: Site Concentrations and Visibility Surrounding America's First City | J. M. Vilbig, V. Sagan, C. Bodine | Journal of Computer Applications in Archaeology | 2026 | 10.5334/jcaa.203 | peer-reviewed | yes (abstract) |
| A critique of 2D coastal-visibility limits formulated in 1901. Proposes **angular** (multidimensional) visibility metrics. | Mediterranean maritime visibility: old limits and new approaches | K. H. Smith, L. Hulin | Antiquity | 2026 | 10.15184/aqy.2026.10340 | peer-reviewed | yes (abstract) |
| A GIS viewshed raster turned into an Unreal Engine texture or mask for first-person and VR exploration. **This is only display**: the visibility is still a 2.5D DEM viewshed and is not computed against 3D buildings. | Bridging archaeological visibility analysis and real-time 3D visualization | G. Malaperdas, G. Delli | Computer Science and Information Technologies 7(1): 93–101 | 2026 | 10.11591/csit.v7i1.pp93-101 | peer-reviewed (minor journal) | yes (abstract) |
| *Baseline*: a 3D visibility analysis chapter (Archaeological 3D GIS, Routledge). | 3D visibility analysis | N. Dell'Unto, G. Landeschi | Archaeological 3D GIS (Routledge), ch. 7 | 2022 | 10.4324/9781003034131-7 | book chapter | no (metadata only) |

**What supersedes the older baselines.** The single binary viewshed on a bare-earth DEM has given way to four refinements:
- probable or fuzzy viewsheds, using Monte Carlo over DEM error and vegetation (Leenders 2025, Sobotkova 2024);
- tests against random or null expectation (Rothenberg 2024, Vilbig 2026);
- built-environment visibility (Routledge and Wilson 2024);
- angular or apparent-size metrics in place of a yes/no answer (Smith and Hulin 2026).

I found no 2024–2026 peer-reviewed paper that runs a probabilistic viewshed over **alternative reconstructed building heights**. That combination is a genuine gap, and Leenders et al. 2025 explicitly name 3D as sparse.

**Implication for Strata 3D**
- Compute visibility on a **DSM = paleo- or modern DEM plus reconstructed building volumes**. Use the variant geometry from section 2, not a single height.
- For tier B, C or D buildings, sample height from the variant's stated range (Monte Carlo, at least 50 runs, a figure reported in older viewshed literature and listed under Unverified) and output a **probable viewshed** of 0 to 1. Report the result as "visible in X% of height hypotheses". This is defensible in front of experts, and it fills the gap identified above.
- Also record the observer height, target height, Earth curvature and refraction setting in the result's legend.
- When claiming that "site X was placed to see Y", include a random-point comparison, following Rothenberg and Vilbig. Expert advisory meetings will ask for this.
- Consider angular size (Smith and Hulin) for "how prominent did the pagoda appear from point P". It is more meaningful than binary visibility.

## 5. Trench stratigraphy in 3D (TIN or voxel) and Harris matrix tools

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| Voxel modelling recommended as **standard practice** for cultural-layer volume and preservation (also listed in section 3). | Predicting Buried Archaeological Heritage in a 3D Environment—Modelling the Urban Subsurface Using Voxels | K. Haase et al. | Geoarchaeology 40(3) | 2025 | 10.1002/gea.70014 | peer-reviewed | yes (abstract) |
| A "stratigraphic biography": depositional events, their temporal and causal ordering, and their rate, using micromorphology, absolute dates and **multidimensional geometric analysis of floors and sedimentary volumes**. | The stratigraphic biography of an archaeological site. Timing depositional events | V. Andreaki, J. A. Barceló | Archaeometry | 2024 | 10.1111/arcm.13049 | peer-reviewed | yes (abstract) |
| **The Harris laws formalized as inferences in CIDOC CRM**, with competency questions and queries. This allows Harris matrices to be mapped to CRM and separates process documentation from reasoning. | Spatio-Temporal Reasoning on Stratigraphic Data in Archaeology: Formalization of the Harris Laws as Inferences Using CIDOC CRM | A. Guillem, M. van Ruymbeke, Ø. Eide, L. De Luca | conference paper (HAL) | 2024 | (no DOI; HAL via OpenAlex) | conference | yes (abstract via OpenAlex) |
| A call for consistent, FAIR recording of stratigraphic and phasing relationships from post-excavation work. | Resurrecting, reinterpreting and reusing stratigraphy: an afterlife for archaeological data | J. S. Taylor, K. May | Antiquity | 2024 | 10.15184/aqy.2024.60 | peer-reviewed | yes (abstract) |
| *Baseline*: a 4D interactive stratigraphy system using spatial DBs and game engines alongside the Harris Matrix. | Integrated and interactive 4D system for archaeological stratigraphy | Ortega-Alvarado et al. | Archaeological and Anthropological Sciences 14: 203 | 2022 | https://digibug.ugr.es/handle/10481/77408 | peer-reviewed | no (snippet only) |

**What supersedes the older baselines.** The Harris Matrix Composer (2008) and similar diagram editors treat the matrix as a drawing. Newer work treats it as a **machine-checkable graph**, either as CRM inference rules (Guillem et al. 2024) or as an EM knowledge graph (EM Manager 2024). It also links the graph to 3D volumes and to absolute timing (Andreaki and Barceló 2024).

**Implication for Strata 3D**
- Store stratigraphic relations (above/below, cuts, fills, equals) as a directed acyclic graph, and **validate it with the Harris laws**, so cycles or contradictions raise an error. Guillem et al. give the formal rules.
- For geometry, use **interface TINs per stratigraphic unit**. Each TIN is interpolated from top and bottom elevations at section and plan points (section 1). Build closed volumes between successive TINs. Add an optional voxel export later, for volume and preservation statistics in the style of Haase.
- Link each 3D volume to its node in the stratigraphic graph. Clicking a layer then highlights it in the Harris view, and the reverse works too.
- Interpolated parts of interfaces, away from section lines, are tier C. Show them semi-transparent or hatched.

## 6. Presentation camera paths and vertical-exaggeration disclosure

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| Neural saliency detection on large heritage point clouds, plus **greedy viewpoint selection** that proposes key views for guided tours. It outperforms handcrafted saliency. | Saliency-Driven View Planning for Cultural Heritage Guided Tours | T. Zhang, S. Filin | ISPRS Archives XLVIII-2/W12-2026: 527 | 2026 | 10.5194/isprs-archives-xlviii-2-w12-2026-527-2026 | conference (ISPRS) | yes (abstract) |
| Virtual tours and digital storytelling as tools for urban heritage (Gdańsk). | Transforming urban experience through virtual tours: a digital storytelling of Gdańsk and its heritage | J. Borucka, F. Picchio | npj Heritage Science | 2026 | 10.1038/s40494-026-02395-w | peer-reviewed | yes (abstract) |
| XR storytelling standards derived from the London Charter and Seville Principles (also listed in section 2). They cover disclosure of how the medium "enhances, bends or distorts" reality. | Standards for science communication in extended and virtual reality… | J. L. Rubio Tamayo et al. | JCOM 23(03) | 2024 | 10.22323/2.23030203 | peer-reviewed | yes (abstract) |
| *Baseline*: the terrain-reversal illusion in shaded relief. About 70% of participants reported "terrain flipping" at least once. | Comparing the terrain reversal effect in satellite images and in shaded relief maps… | (authors not verified) | International Journal of Digital Earth | 2018 | 10.1080/17538947.2018.1447030 | peer-reviewed | no (snippet only) |
| *Baseline*: a relief-shading survey in which the 5× vertical-exaggeration variant was the most polarizing ("appealing" against "artificial"). | Comparison of Relief Shading Techniques Applied to Landforms | (authors not verified) | ISPRS Int. J. Geo-Information 9(4): 253 | 2020 | https://www.mdpi.com/2220-9964/9/4/253 | peer-reviewed | no (snippet only) |

**Gap.** I made three attempts across WebSearch, Crossref and OpenAlex and found **no 2024–2026 user study on vertical-exaggeration perception** in 3D terrain or heritage display. The evidence base is pre-2024 cartography plus the general disclosure principles of the charters and Rubio Tamayo 2024.

**Implication for Strata 3D**
- Show the **VE factor permanently** in the viewport and in every exported image or video, for example "Vertical exaggeration ×2.0", next to the scale bar and north arrow. Default to ×1.0 for anything presented as analysis. Allow VE above 1 only in a "presentation" mode that stamps the factor on screen. Viewshed and slope results must always be computed at ×1, whatever the display setting.
- Each camera path keyframe should store the camera pose, the VE value, which variant (A or B) and which tiers are visible, and the active time layer. A recorded talk is then reproducible and auditable, which is the paradata idea applied to presentation.
- Zhang and Filin support **auto-suggested key views**. As a first step, Strata can propose viewpoints with simpler heuristics, such as framing each site plus its relation line to the trench, and leave the final choice to the expert.

## 7. Uncertainty and hypothesis visualization in virtual reconstruction

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| A **user-independent, traceable formula** that turns per-element values from an uncertainty scale into one **average uncertainty** number for a whole model. It complements the false-colour scale. | Quantifying Uncertainty in Hypothetical 3D Reconstruction—A User-Independent Methodology for the Calculation of Average Uncertainty | R. Foschi, F. Fallavollita, F. I. Apollonio | Heritage 7(8): 4440–4454 | 2024 | 10.3390/heritage7080209 | peer-reviewed | yes (abstract) |
| A source-based **multi-feature** uncertainty analysis at **urban scale**, with false-colour shading and an average uncertainty **weighted by volume** (Piazza delle Erbe, Verona). | Multi-Feature Uncertainty Analysis for Urban-Scale Hypothetical 3D Reconstructions: Piazza delle Erbe Case Study | F. I. Apollonio, F. Fallavollita, R. Foschi, R. Smurra | Heritage 7(1): 476–498 | 2024 | 10.3390/heritage7010023 | peer-reviewed | yes (abstract) |
| An open-access plugin for uncertainty assessment, from the same group. | An Integrated and Open-Access Plugin for Uncertainty Assessment of the Hypothetical Virtual Reconstruction of Architecture | F. I. Apollonio, F. Fallavollita, R. Foschi | LNCS (3D Research Challenges in CH VI): 175–188 | 2025 | 10.1007/978-3-032-05656-6_16 | conference/book chapter | no (metadata only) |
| Evidence Support Level mapped to maximum LoD (ECLA; also listed in section 2). | An evidence-constrained method for LOD reachability assessment… | Z. Guo et al. | npj Heritage Science | 2026 | 10.1038/s40494-026-02953-2 | peer-reviewed | yes (abstract) |
| A source-level layer (LoRef) as the primary information, with reliability derived afterwards (also listed in section 2). | From sources to Levels of Reference (LoRef)… | E. C. Giovannini, J. Bono | ISPRS Archives | 2026 | 10.5194/isprs-archives-xlviii-2-w12-2026-167-2026 | conference | yes (abstract) |
| Paradata in 3D scholarship revisited (book chapter). | A Leap of Faith: Revisiting Paradata in 3D Scholarship | C. Papadopoulos | In: Paradata (Springer, Knowledge Management and Organizational Learning) | 2024 | 10.1007/978-3-031-53946-6_4 | book chapter | no (metadata only) |

**What supersedes the older baselines.** The London Charter and Seville Principles call for "distinguishing evidence from hypothesis" but give no metric. The 2024–2026 work adds three things:
- (a) a defined ordinal **scale of uncertainty** with **false-colour** rendering;
- (b) a **reproducible scene-level number**, the average uncertainty, which can be weighted by volume;
- (c) evidence-capped LoD.

**Implication for Strata 3D**
- Strata's four tiers (A to D) are a compact ordinal scale. Keep them, and document the mapping to the finer Bologna/CoVHer scale for interoperability (the exact scale levels were not verified from the abstracts; check the full text first).
- Calculate and display **one scene-level number**: the volume-weighted mean tier of the visible objects, following Foschi 2024 and Apollonio 2024. Example: "Scene evidence index: 2.7 (A=1 … D=4)". Put it on the slide footer for talks.
- Rendering rules:
  - false-colour or hue per tier in "analysis" mode;
  - natural materials with an outline or hatch overlay in "presentation" mode;
  - an LoD cap per tier (ECLA).
- Never use photoreal texture on tier C or D.
- Hypothesis switching (A/B) must keep the tier colouring, so the audience always sees which variant is better supported.

## 8. Gaussian splatting, NeRF and photogrammetry for heritage capture

| Finding / method | Title | Authors | Venue | Year | DOI / URL | Type | Verified |
|---|---|---|---|---|---|---|---|
| 3DGS clearly beats photogrammetry for rendering and real-time display, but its **geometric accuracy is slightly lower**, with a larger gap on small or low-resolution datasets. Postshot and LichtFeld Studio were more robust than the original GraphDeco code. | Photogrammetry and 3D Gaussian Splatting for Cultural Heritage. Pros, Cons and Main Differences | X. Li, A. Martino, F. Chiabrando, X. Li | ISPRS Archives XLIX-B2-2026: 451–458 | 2026 | 10.5194/isprs-archives-XLIX-B2-2026-451-2026 | conference (ISPRS) | yes (page) |
| **Expert study** (8 local experts, Puri Klungkung, Bali). 3DGS was preferred for close-ups of vegetation, water and detail. **Photogrammetry was preferred for scholarly dissemination** because of its geometric accuracy. 3DGS suits public-facing projects. | Towards a human-centered evaluation of 3D Gaussian Splatting for cultural heritage visualization | F. H. Arief, K. A. Satriadi, E. M. Champion | Virtual Archaeology Review 17(35) | 2026 | 10.4995/var.2024.25551 | peer-reviewed | yes (abstract) |
| SfM-MVS, NeRF and GS with low-cost sensors (drone, 360° camera), checked against TLS. **SfM-MVS gave the highest geometric precision**. NeRF and GS "fell short of the accuracy required" but were faster and better for visualization. | 3D representation of Architectural Heritage: a comparative analysis of NeRF, Gaussian Splatting, and SfM-MVS reconstructions using low-cost sensors | P. Clini, R. Nespeca, R. Angeloni, L. Coppetta | ISPRS Archives XLVIII-2/W8-2024: 93 | 2024 | 10.5194/isprs-archives-xlviii-2-w8-2024-93-2024 | conference (ISPRS) | yes (abstract) |
| Artifacts under 5 cm, with a metrology-arm ground truth (RMS, Chamfer). NeRF beat GS geometrically, but **both were below SfM**. | Comparison of NeRF- and SfM-Based Methods for Point Cloud Reconstruction for Small-Sized Archaeological Artifacts | M. Á. Maté-González, R. Yali, J. Rodríguez-Hernández, E. González-González, J. Aguirre de Mata | Remote Sensing 17(14): 2535 | 2025 | 10.3390/rs17142535 | peer-reviewed | yes (abstract) |
| 3DGS vs LiDAR (Bouwpub). 3DGS has better visual expressiveness, while LiDAR has better structural accuracy and segmentation. Proposes **3DGS as a complementary visualization module**, through Blender multi-angle rendering and an LOD3DGS pipeline. | From comparison to integration: A workflow evaluation of 3D Gaussian splatting and LiDAR point cloud for modern architectural heritage | Y. Yu, E. Verbree, P. van Oosterom, U. Pottgiesser, Y. Peng, F. Poux | Automation in Construction 180: 106509 | 2025 | 10.1016/j.autcon.2025.106509 | peer-reviewed | yes (abstract via OpenAlex) |
| Transparent glass artifact. **2DGS gives the best accuracy and appearance trade-off**. 3DGS adds surface artifacts that harm metric reliability, and photogrammetry fails. "Visual quality does not ensure geometric accuracy." | Metric Error Assessment Regarding Geometric 3D Reconstruction of Transparent Surfaces via SfM Enhanced by 2D and 3D Gaussian Splatting | D. Billi, G. Caroti, A. Piemonte | Sensors 25(14): 4410 | 2025 | 10.3390/s25144410 | peer-reviewed | yes (abstract) |
| MILo (mesh-in-the-loop GS) vs Metashape MVS, against a TLS reference using M3C2. MVS is slightly better on stable architecture. MILo has far fewer outliers on vegetation, reflective ceramic and thin structures. | 3D Meshing of Challenging Surfaces using Gaussian Splatting | D. Billi, C. Delasse, A. Murtiyoso, H. Macher, P. Grussenmeyer, G. Caroti, A. Piemonte | ISPRS Archives XLIX-B2-2026: 801–808 | 2026 | 10.5194/isprs-archives-xlix-b2-2026-801-2026 | conference (ISPRS) | yes (abstract) |
| Underwater shipwreck: SfM is better for structure, while 3DGS gives more photorealistic texture and handles lighting distortion. | 3D Reconstruction of Underwater Shipwrecks: 3D Gaussian Splatting and Structure from Motion for the Melania shipwreck | D. Billi, V. Croce, A. Piemonte, G. Caroti | ISPRS Archives XLVIII-2/W10-2025: 19–25 | 2025 | 10.5194/isprs-archives-XLVIII-2-W10-2025-19-2025 | conference (ISPRS) | yes (page) |
| Photogrammetry, NeRF and 3DGS in a restoration context. Photogrammetry remains the standard but struggles on reflective, transparent and textureless surfaces. No metrics are given in the abstract. | A comparative evaluation of 3D reconstruction with photogrammetry, NeRFs and 3D Gaussian Splatting in Cultura[l] Heritage Restoration | M. Carmeliti, S. Marziali | ISPRS Archives XLVIII-2/W12-2026: 89–96 | 2026 | 10.5194/isprs-archives-XLVIII-2-W12-2026-89-2026 | conference (ISPRS) | yes (page) |
| 3DGS training on 6 datasets (Córdoba Mosque–Cathedral). **90–95% of perceptual improvement comes within 10k–20k iterations**. | Convergence behaviour and computational efficiency of Gaussian Splatting in complex heritage environments | R. Ortiz-Cordero, R. E. Hidalgo Fernández | Virtual Archaeology Review | 2026 | 10.4995/var.2026.25814 | peer-reviewed | yes (abstract) |
| LiDAR-initialized Gaussians plus illumination-aware appearance. Improves geometric accuracy on weakly textured heritage surfaces (no numbers given in the abstract). | LiDAR-Guided Illumination-Aware 3D Gaussian Splatting for Cultural Heritage | X. Liu, X. Li, W. Li, T. Liu, W. Sun, S. Zhang | ISPRS Annals XI-2-2026: 561 | 2026 | 10.5194/isprs-annals-xi-2-2026-561-2026 | conference (ISPRS Annals) | yes (abstract) |
| UAV photogrammetry plus hybrid 3DGS in UE5 for **East Asian timber-and-stone architecture** (glazed tiles are non-Lambertian). Opacity culling removes floaters, and rendering exceeds 110 FPS at "visualization-grade" relative geometry. | Digital regeneration of historical buildings using UAV photogrammetry and 3D Gaussian splatting for metaverse environments | J. Hu, Z. Jiang, Y. Zhang | npj Heritage Science 14 | 2026 | 10.1038/s40494-026-02649-7 | peer-reviewed | yes (abstract via OpenAlex) |
| A modified 3DGS for artifact documentation that addresses camera-alignment errors and background clutter, with generative gap filling. No metrics are given in the abstract. | 3D Gaussian Splatting for Enhanced Documentation of Cultural Artifacts | J. Lee, S. Park, J. Min, J. Choi | ISPRS Annals X-M-2-2025: 215–222 | 2025 | 10.5194/isprs-annals-X-M-2-2025-215-2025 | conference (ISPRS Annals) | yes (page) |
| Semantic-aware 3DGS compression for mobile AR. Storage falls from 873.9 MB to 69.0 MB at 30.22 dB PSNR, and inscriptions and cracks are preserved. | HG-GS: Semantic-Aware 3D Gaussian Splatting Compression for Mobile AR Heritage Visualization | B. Zhang, E. Pi, Y. Zheng, R. Shi | SSRN | 2026 | 10.2139/ssrn.6756958 | **preprint** | yes (abstract) |

**Consensus, 2024–2026, depends on surface type.**
- **Opaque, well-textured surfaces:** SfM-MVS, TLS or LiDAR remains the geometric reference, and 3DGS and NeRF win on rendering quality and speed (Clini 2024, Maté-González 2025, Li 2026, Yu 2025, Melania 2025).
- **Transparent, reflective, vegetated or thin structures:** the newer variants match or beat SfM on geometry. These are 2DGS (Billi 2025, where photogrammetry failed entirely), MILo (Billi 2026, with far fewer outliers on vegetation and ceramics) and LiDAR-initialized 3DGS (Liu 2026).
- The expert study (Arief 2026) reaches an independent split: photogrammetry for scholarship, 3DGS for public-facing work.
- For Gyeongju, grass-covered tumuli, trees and glazed or tiled roofs are exactly the second group of surfaces.

**Implication for Strata 3D**
- Rendering an imported splat should never upgrade an object's tier. A 3DGS capture of a standing monument is **tier A for appearance but not for measurement**. Measured values such as heights for viewshed or elevations must come from the mesh, point cloud or DEM layer.
- Adopt a **two-layer model**: a metric layer (mesh, DEM or point cloud, used for all analysis) and an optional splat "appearance" layer for presentation mode, as Yu 2025 proposes. Use 3DGS for current-state site context such as tumuli fields and vegetation, and never for reconstructions.
- If Strata adds splat rendering (for example a `.ply` or `.spz` Gaussian loader in the OpenGL viewport), budget for LOD and compression (LOD3DGS, HG-GS-style pruning). Hu 2026 shows real-time rates in UE5 for East Asian timber architecture, which suggests Gyeongju assets are feasible.
- For any capture work the user commissions, Ortiz-Cordero 2026 suggests 10k–20k iterations is enough for preview-quality splats.

---

## Unverified

These were seen only in search-engine snippets, or could not be opened. They are not cited as evidence above.

- **"Baradaran Rahimi et al., npj Heritage Science 2025"**, a photogrammetry vs 3DGS comparison. The snippet's described findings, that 2DGS is the best trade-off with 3DGS surface artifacts, match Billi et al., *Sensors* 2025 almost word for word. This is likely a search-snippet conflation, so do not cite it.
- Rodríguez-Moreno (2024), "Depicting the Uncertain in Virtual Reconstructions of Architectural Heritage", Univ. Granada record (https://produccioncientifica.ugr.es/documentos/668c296648730a0be66d2f5d). Not opened.
- Cazzaro (2025), a book on the evaluation of source-based 3D reconstructions (arthistoricum.net). Not opened.
- Barceló and Fallavollita (2025), on a chromatic scale of uncertainty. Seen in a Bologna CRIS listing only.
- "A perceptual assessment of the chromatic scale of uncertainty in hypothetical virtual architectural 3D reconstruction" (Apollonio et al., listed as 2024 in the Bologna CRIS). Not found in Crossref.
- A 2026 Springer chapter on CoVHer open-source tools for geometric proofing and uncertainty assessment. Seen in a snippet only.
- The Reboulaki thesis (Technical University of Crete), on Bayesian uncertainty visualization. Year unknown.
- Ortega-Alvarado et al. (2022), Archaeol. Anthropol. Sci. 14:203, a 4D stratigraphy system. Pre-2024, snippet only.
- Aachen paleotopography (Journal of Maps 2019, 10.1080/17445647.2019.1590248). Snippet only. One search snippet called it a "Rome voxel paleotopography", which is a mis-label, so do not cite it as a Rome study.
- The Pompeii/Sarno pre-AD 79 paleo-surface from about 1,800 drill cores using CART (EGU 2010 abstract). Pre-2024, snippet only.
- Gela, Sicily, borehole paleoenvironment study (MDPI Heritage 9(1):41, about 2026). Snippet only.
- The "at least 50 Monte Carlo runs to stabilize fuzzy visibility" figure, from an older viewshed paper (academia.edu). Snippet only. Use it as a starting default, not as a cited fact.
- The 2018 IJDE terrain-reversal study and the 2020 ISPRS IJGI relief-shading survey (section 6 baselines). Snippet only.
- The Harris Matrix Data Package (hmdp, Codeberg), harris-mcp (GitHub) and the "Strati5" mobile app. These are software listings, not peer-reviewed, and their dates are unverified.
- Guillem et al. (2025), Notre-Dame hypothesis versioning, and the CoVHer standards (2025), the Apollonio plugin (2025) and Papadopoulos (2024). Their existence is verified, but no abstract was available, so their content is unverified (marked "metadata only" above).

## Search notes

- I used WebSearch (standard and extended), WebFetch on ISPRS/Copernicus pages, and Crossref and OpenAlex APIs through the local helper scripts `cr.py` and `oa.py` in the same scratchpad folder.
- Thin areas:
  - 2024–2026 vertical-exaggeration perception (none found);
  - probabilistic viewshed over reconstructed building heights (none found, a research gap);
  - new peer-reviewed Harris matrix software (the field has moved to graph and ontology formalization instead).
