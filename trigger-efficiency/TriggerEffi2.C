#include <TFile.h>
#include <TTree.h>
#include <TChain.h>
#include <TH1F.h>
#include <TEfficiency.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <utility>
#include <string>
#include <vector>
#include <deque>
#include <cmath>

using json = nlohmann::json;

// ------------------------------------------------------------
// Cargar JSON (Golden JSON de CMS)
// ------------------------------------------------------------
std::set<std::pair<unsigned int, unsigned int>> LoadJSON(const char *jsonFile) {
    std::set<std::pair<unsigned int, unsigned int>> goodLumis;
    std::ifstream in(jsonFile);
    if (!in.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir el archivo JSON: " << jsonFile << std::endl;
        return goodLumis;
    }

    json j;
    in >> j;
    for (auto it = j.begin(); it != j.end(); ++it) {
        unsigned int run = std::stoul(it.key());
        for (auto &range : it.value()) {
            unsigned int start = range[0];
            unsigned int end = range[1];
            for (unsigned int l = start; l <= end; ++l) {
                goodLumis.insert({run, l});
            }
        }
    }

    std::cout << "[OK] JSON cargado: " << goodLumis.size() << " pares (run, lumi) validos." << std::endl;
    return goodLumis;
}

// ------------------------------------------------------------
// Separar una cadena separada por comas en un vector de strings,
// eliminando espacios en blanco al inicio/fin de cada elemento
// ------------------------------------------------------------
std::vector<std::string> SplitTriggerList(const std::string &input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        if (start != std::string::npos) {
            result.push_back(item.substr(start, end - start + 1));
        }
    }
    return result;
}

// ------------------------------------------------------------
// Script principal
//
// La eficiencia de cada trigger en "triggersToStudy" se mide condicionando
// el denominador a eventos que ademas dispararon "referenceTrigger".
// El trigger de referencia debe ser uno cuya eficiencia ya este saturada (en su plateau)
// en la region de pT que se quiere estudiar.
// ------------------------------------------------------------
void TriggerEffi2(const char *triggersToStudy = "HLT_AK8PFJet360_TrimMass30",
                   const char *referenceTrigger = "HLT_AK8PFJet140",
                   const char *treeName = "Events",
                   const char *jsonFile = "/code/Cert_271036-284044_13TeV_Legacy2016_Collisions16_JSON.txt") {

    const char* baseDir = "/code/Datasets/JetHT30558/";

    // --- LISTA DE 24 ARCHIVOS (subconjunto de los 72) ---
    const char* files[] = {
        "0290F73B-A51C-A441-AEC1-8429F9CC8AA8.root","309C3921-6FC0-514D-A107-C068D2AA05B0.root",
        "42FD9F80-A8FC-9340-A37B-2625360B08DD.root","4B42C893-C9F8-CC4A-8185-683163215D2E.root",
        "06E32D6A-F2FF-3E4A-A835-A09519142697.root","61D13DC9-20AB-7B4E-9517-AADF66C61900.root",
        "72A17A39-18AC-5645-B715-DE98C0AA3D69.root","877B9D74-92FD-4E46-8024-F315CF37C8E2.root",
        "933DD99E-EE39-974E-89B7-C4549F23FB8B.root","ACAA748A-B023-D941-BBA4-4F3A1F01B27A.root",
        "B59FE5FA-AC78-4F4F-8BC9-78DC627EC9F8.root","B88AC6C4-2AC4-2442-8A0A-D0E986D00C55.root",
        "C3C35D40-5D97-4E45-B9A1-4D77F0F0C367.root","D8DB7DF1-EF40-BA40-AF21-1E01A783B4AA.root",
        "E66CBBD0-7DD3-A143-9D45-DFB2445FAD42.root","1854E8D3-B29F-064D-BEF5-F69E755E17CE.root",
        "1C9E31AA-BECF-DE4C-915B-E298D0DBC425.root","2CC82454-01CF-494B-825E-C0C5D5E452CD.root",
        "7B8F1E06-17C4-714F-BDF1-0C1E6ED14B9D.root","C05051D5-9CEC-544E-B851-058CB40F5392.root",
        "E545F167-3D22-7B4D-ABE5-01F5AA64854F.root","EFD7B1BD-64D0-EB43-BCBA-5415DE0FB5CB.root",
        "2D96AD66-F631-BE45-9791-BBABF8A640B7.root","3FB02BB9-DE74-AC48-B90A-44F69624BBD1.root"
    };
    const int nFiles = sizeof(files) / sizeof(files[0]);

    // Lista de triggers a estudiar (uno o varios, separados por coma)
    std::vector<std::string> triggerList = SplitTriggerList(triggersToStudy);
    const int nTriggers = triggerList.size();

    if (nTriggers == 0) {
        std::cerr << "[ERROR] No se especifico ningun trigger valido en triggersToStudy." << std::endl;
        return;
    }

    std::cout << "[INFO] Trigger de referencia: " << referenceTrigger << std::endl;
    std::cout << "[INFO] Triggers a estudiar (" << nTriggers << "):" << std::endl;
    for (const auto &t : triggerList) std::cout << "       - " << t << std::endl;

    // Crear el TChain con los 24 archivos
    TChain *tree = new TChain(treeName);
    for (int i = 0; i < nFiles; ++i) {
        tree->Add((std::string(baseDir) + files[i]).c_str());
    }

    if (tree->GetNtrees() != nFiles) {
        std::cerr << "[ERROR] Se esperaban " << nFiles << " archivos en el TChain, pero se cargaron "
                   << tree->GetNtrees() << ". Revisa la ruta: " << baseDir << std::endl;
        return;
    }
    std::cout << "[OK] Archivos anadidos al TChain: " << tree->GetNtrees() << std::endl;

    // Cargar JSON
    auto goodLumis = LoadJSON(jsonFile);
    if (goodLumis.empty()) {
        std::cerr << "[WARN] El JSON esta vacio o no se cargo. Continuando sin filtro de JSON." << std::endl;
    }

    // Variables del arbol
    UInt_t run = 0, lumi = 0, nFatJet = 0;
    Float_t FatJet_pt[128];
    Bool_t refPass = false;
    std::deque<Bool_t> trigPass(nTriggers, false);

    tree->SetBranchAddress("run", &run);
    tree->SetBranchAddress("luminosityBlock", &lumi);
    tree->SetBranchAddress("nFatJet", &nFatJet);
    tree->SetBranchAddress("FatJet_pt", FatJet_pt);
    tree->SetBranchAddress(referenceTrigger, &refPass);
    for (int t = 0; t < nTriggers; ++t) {
        tree->SetBranchAddress(triggerList[t].c_str(), &trigPass[t]);
    }

    Long64_t nentries = tree->GetEntries();
    std::cout << "[INFO] Eventos totales en el TChain: " << nentries << std::endl;

    // Histogramas: denominador comun (condicionado al trigger de referencia)
    // y un numerador por cada trigger estudiado
    TH1F *h_all = new TH1F("h_all", "FatJet pT (denominador, condicionado a referencia);p_{T} [GeV];Eventos", 30, 200, 1200);
    std::vector<TH1F*> h_pass(nTriggers);
    for (int t = 0; t < nTriggers; ++t) {
        std::string hname = "h_pass_" + triggerList[t];
        h_pass[t] = new TH1F(hname.c_str(),
                              (triggerList[t] + ";p_{T} [GeV];Eventos").c_str(),
                              30, 200, 1200);
    }

    Long64_t nsel = 0;
    for (Long64_t i = 0; i < nentries; ++i) {
        if (i % 200000 == 0) std::cout << "Procesando evento " << i << " / " << nentries << std::endl;
        tree->GetEntry(i);

        if (!goodLumis.empty() && goodLumis.find({run, lumi}) == goodLumis.end()) continue;
        if (nFatJet < 1) continue;
        if (!refPass) continue;  // condicion clave: solo eventos que pasan el trigger de referencia

        Float_t leading_pt = FatJet_pt[0];
        h_all->Fill(leading_pt);

        for (int t = 0; t < nTriggers; ++t) {
            if (trigPass[t]) h_pass[t]->Fill(leading_pt);
        }
        nsel++;
    }

    std::cout << "[OK] Eventos seleccionados (JSON, >=1 FatJet, pasan referencia): " << nsel << std::endl;

    // Calcular disposicion del canvas combinado segun el numero de triggers
    int nCols = (nTriggers > 1) ? 2 : 1;
    int nRows = (int)std::ceil((double)nTriggers / nCols);

    TCanvas *c1 = new TCanvas("c1", "Trigger Efficiency", 700 * nCols, 600 * nRows);
    if (nTriggers > 1) c1->Divide(nCols, nRows);
    gStyle->SetOptStat(0);

    std::vector<TEfficiency*> effs(nTriggers, nullptr);

    for (int t = 0; t < nTriggers; ++t) {
        if (nTriggers > 1) c1->cd(t + 1);
        else c1->cd();
        gPad->SetGrid();

        if (TEfficiency::CheckConsistency(*h_pass[t], *h_all)) {
            effs[t] = new TEfficiency(*h_pass[t], *h_all);
            std::string title = "Eficiencia de " + triggerList[t] + ";FatJet p_{T} [GeV];Eficiencia";
            effs[t]->SetTitle(title.c_str());
            effs[t]->SetMarkerStyle(20);
            effs[t]->SetMarkerColor(kBlue + 1);
            effs[t]->SetLineColor(kBlue + 1);
            effs[t]->Draw("AP");
        } else {
            std::cerr << "[ERROR] Histogramas inconsistentes para TEfficiency en " << triggerList[t] << std::endl;
        }
    }

    std::string combinedName = "TriggerEffi2_combined.png";
    c1->SaveAs(combinedName.c_str());
    std::cout << "[OK] Grafico combinado guardado: " << combinedName << std::endl;

    // Ademas, guardar cada trigger en su propio archivo PNG individual
    for (int t = 0; t < nTriggers; ++t) {
        if (!effs[t]) continue;
        TCanvas *cInd = new TCanvas(("c_" + triggerList[t]).c_str(), triggerList[t].c_str(), 800, 700);
        cInd->SetGrid();
        effs[t]->Draw("AP");
        std::string outname = "TriggerEffi2_" + triggerList[t] + "_ref_" + std::string(referenceTrigger) + ".png";
        cInd->SaveAs(outname.c_str());
        std::cout << "[OK] Grafico guardado: " << outname << std::endl;
    }
}
