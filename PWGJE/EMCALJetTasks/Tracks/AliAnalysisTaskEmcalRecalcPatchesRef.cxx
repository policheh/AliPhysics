/************************************************************************************
 * Copyright (C) 2017, Copyright Holders of the ALICE Collaboration                 *
 * All rights reserved.                                                             *
 *                                                                                  *
 * Redistribution and use in source and binary forms, with or without               *
 * modification, are permitted provided that the following conditions are met:      *
 *     * Redistributions of source code must retain the above copyright             *
 *       notice, this list of conditions and the following disclaimer.              *
 *     * Redistributions in binary form must reproduce the above copyright          *
 *       notice, this list of conditions and the following disclaimer in the        *
 *       documentation and/or other materials provided with the distribution.       *
 *     * Neither the name of the <organization> nor the                             *
 *       names of its contributors may be used to endorse or promote products       *
 *       derived from this software without specific prior written permission.      *
 *                                                                                  *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND  *
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED    *
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
 * DISCLAIMED. IN NO EVENT SHALL ALICE COLLABORATION BE LIABLE FOR ANY              *
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES       *
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;     *
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND      *
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS    *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                     *
 ************************************************************************************/
#include <algorithm>
#include <array>
#include <iostream>
#include <unordered_map>
#include <sstream>
#include <string>
#include <TClonesArray.h>
#include <THistManager.h>
#include <TLinearBinning.h>
#include <TList.h>
#include <TString.h>
#include "AliAnalysisManager.h"
#include "AliEMCALTriggerPatchInfo.h"
#include "AliAnalysisTaskEmcalRecalcPatchesRef.h"

ClassImp(EMCalTriggerPtAnalysis::AliAnalysisTaskEmcalRecalcPatchesRef);

using namespace EMCalTriggerPtAnalysis;

AliAnalysisTaskEmcalRecalcPatchesRef::AliAnalysisTaskEmcalRecalcPatchesRef():
  AliAnalysisTaskEmcalTriggerBase(),
  fEnableSumw2(false),
  fOnlineThresholds()
{

}

AliAnalysisTaskEmcalRecalcPatchesRef::AliAnalysisTaskEmcalRecalcPatchesRef(const char * name):
  AliAnalysisTaskEmcalTriggerBase(name),
  fEnableSumw2(false),
  fOnlineThresholds()
{
  fOnlineThresholds.Set(8);
}

void AliAnalysisTaskEmcalRecalcPatchesRef::CreateUserHistos(){
  const std::array<std::string, 9> kNamesTriggerClasses = {{"MB", "EG1", "EG2", "DG1", "DG2", "EJ1", "EJ2", "DJ1", "DJ2"}};
  const std::array<std::string, 4> kNamesPatchTypes = {{"EGA", "DGA", "EJE", "DJE"}};

  TLinearBinning ADCBinning(2000, 0., 2000.), colbinning(48, -0.5, 47.5), rowbinning(104, -0.5, 103.5);
  const TBinning *firedpatchbinning[3] = {&ADCBinning, &colbinning, &rowbinning};
  // Create event counters
  for(const auto &kt : kNamesTriggerClasses) fHistos->CreateTH1(Form("hEventCounter%s", kt.data()), Form("Event counter for %s", kt.data()), 1, 0.5, 1.5);

  // Min. Bias: Create patch energy spectra (all patches) for EMCAL and DCAL
  for(const auto &kp : kNamesPatchTypes) fHistos->CreateTH1(Form("hPatchADC%sMB", kp.data()), Form("Patch ADC spectra for %s patches in MB events", kp.data()), 2000, 0., 2000., fEnableSumw2 ? "s" : "");

  // Triggers: Create trigger spectra and THnSparse of firing patches
  for(const auto &kt : kNamesTriggerClasses) {
    if(kt == "MB") continue;
    const char detector = kt[0];
    const char *patchtype = ((kt[1] == 'G') ? "GA" : "JE");
    fHistos->CreateTH1(Form("hPatchADC%c%s%s", detector, patchtype, kt.data()), Form("Patch ADC spectra for %c%s patches in %s events", detector, patchtype, kt.data()), 2000, 0., 2000., fEnableSumw2 ? "s" : "");
    fHistos->CreateTHnSparse(Form("hFiredPatches%c%s%s", detector, patchtype, kt.data()), Form("Fired %c%s patches for trigger %s", detector, patchtype, kt.data()), 3, firedpatchbinning, fEnableSumw2 ? "s" : "");
  }
}

void AliAnalysisTaskEmcalRecalcPatchesRef::UserFillHistosAfterEventSelection(){
  const std::array<std::string, 9> kNamesTriggerClasses = {{"MB", "EG1", "EG2", "DG1", "DG2", "EJ1", "EJ2", "DJ1", "DJ2"}};
  for(const auto &kt : kNamesTriggerClasses) {
    if(std::find(fSelectedTriggers.begin(), fSelectedTriggers.end(), kt) != fSelectedTriggers.end()) fHistos->FillTH1(Form("hEventCounter%s", kt.data()), 1.);
  }
}

bool AliAnalysisTaskEmcalRecalcPatchesRef::Run(){
  const std::array<std::string, 9> kNamesTriggerClasses = {{"MB", "EG1", "EG2", "DG1", "DG2", "EJ1", "EJ2", "DJ1", "DJ2"}};
  const std::unordered_map<std::string, ETriggerThreshold_t> kPatchIndex = {{"EG1", kThresholdEG1},{"EG2", kThresholdEG2},{"DG1", kThresholdDG1},{"DG2", kThresholdDG2},
                                                                            {"EJ1", kThresholdEJ1},{"EJ2", kThresholdEJ2},{"DJ1", kThresholdDJ1},{"DJ2", kThresholdDJ2}};
  if(!fSelectedTriggers.size()) return false;       // no trigger selected

  auto findTriggerType = [](const std::vector<TString> &triggers, TString type) -> bool {
    bool found = false;
    for(const auto t : triggers) {
      if(t.Contains(type)) {
        found = true;
        break;
      }
    }
    return false;
  };
  bool isMB = std::find(fSelectedTriggers.begin(), fSelectedTriggers.end(), "MB") != fSelectedTriggers.end(),
       isEG = findTriggerType(fSelectedTriggers, "EG"),
       isDG = findTriggerType(fSelectedTriggers, "EG"),
       isEJ = findTriggerType(fSelectedTriggers, "EG"),
       isDJ = findTriggerType(fSelectedTriggers, "EG");

  std::vector<AliEMCALTriggerPatchInfo *> EGApatches, DGApatches, EJEpatches, DJEpatches;
  if(isMB || isEG) EGApatches = SelectAllPatchesByType(*fTriggerPatchInfo, kEGApatches);
  if(isMB || isDG) DGApatches = SelectAllPatchesByType(*fTriggerPatchInfo, kDGApatches);
  if(isMB || isEJ) EJEpatches = SelectAllPatchesByType(*fTriggerPatchInfo, kEJEpatches);
  if(isMB || isDJ) DJEpatches = SelectAllPatchesByType(*fTriggerPatchInfo, kDJEpatches);
  
  for(const auto &t : fSelectedTriggers) {
    if(!std::find(kNamesTriggerClasses.begin(), kNamesTriggerClasses.end(), t.Data())) continue;
    if(t == "MB") {
      // Min bias: Only fill patch ADC spectra all patches
      for(auto patch :  EGApatches) fHistos->FillTH1("hPatchADCEGAMB", patch->GetADCAmp()); 
      for(auto patch :  DGApatches) fHistos->FillTH1("hPatchADCDGAMB", patch->GetADCAmp()); 
      for(auto patch :  EJEpatches) fHistos->FillTH1("hPatchADCEJEMB", patch->GetADCAmp()); 
      for(auto patch :  DJEpatches) fHistos->FillTH1("hPatchADCDJEMB", patch->GetADCAmp()); 

      continue;
    } else {
      const char detector = t[0];
      const char *patchtype = ((t[1] == 'G') ? "GA" : "JE");
      std::vector<AliEMCALTriggerPatchInfo *> &patchhandler = (detector == 'E' ? (t[1] == 'G' ? EGApatches : EJEpatches) : (t[1] == 'G' ? DGApatches : DJEpatches)); 
      for(auto p : patchhandler) fHistos->FillTH1(Form("hPatchADC%c%s%s", detector, patchtype, t.Data()), p->GetADCAmp());
      auto firedpatches = SelectFiredPatchesByTrigger(*fTriggerPatchInfo, kPatchIndex.find(t.Data())->second);
      for(auto p : firedpatches) {
        double point[3] = {static_cast<double>(p->GetADCAmp()), static_cast<double>(p->GetColStart()), static_cast<double>(p->GetRowStart())};
        fHistos->FillTHnSparse(Form("hFiredPatches%c%s%s", detector, patchtype, t.Data()), point);
      }
    }
  }
  return true;
}

std::vector<AliEMCALTriggerPatchInfo *> AliAnalysisTaskEmcalRecalcPatchesRef::SelectAllPatchesByType(const TClonesArray &list, EPatchType_t patchtype) const {
  std::vector<AliEMCALTriggerPatchInfo *> result;
  for(auto p : list){
    AliEMCALTriggerPatchInfo *patch = static_cast<AliEMCALTriggerPatchInfo *>(p);
    if(!patch->IsRecalc()) continue;
    bool selected(true);
    switch(patchtype){
    case kEGApatches: if(patch->IsDCalPHOS() || !patch->IsGammaHighRecalc()) selected = false; break;
    case kDGApatches: if(patch->IsEMCal() || !patch->IsGammaHighRecalc()) selected = false; break;
    case kEJEpatches: if(patch->IsDCalPHOS() || !patch->IsJetHighRecalc()) selected = false; break;
    case kDJEpatches: if(patch->IsEMCal() || !patch->IsJetHighRecalc()) selected = false; break;
    if(selected) result.emplace_back(patch);
    };
  }
  return result;
}

std::vector<AliEMCALTriggerPatchInfo *> AliAnalysisTaskEmcalRecalcPatchesRef::SelectFiredPatchesByTrigger(const TClonesArray &list, ETriggerThreshold_t trigger) const {
  std::vector<AliEMCALTriggerPatchInfo *> result;
  EPatchType_t patchtype;
  switch(trigger) {
  case kThresholdEG1: patchtype = kEGApatches; break;
  case kThresholdEG2: patchtype = kEGApatches; break;
  case kThresholdDG1: patchtype = kDGApatches; break;
  case kThresholdDG2: patchtype = kDGApatches; break;
  case kThresholdEJ1: patchtype = kEJEpatches; break;
  case kThresholdEJ2: patchtype = kEJEpatches; break;
  case kThresholdDJ1: patchtype = kDJEpatches; break;
  case kThresholdDJ2: patchtype = kDJEpatches; break;
  };
  for(auto patch : SelectAllPatchesByType(list, patchtype)) {
    if(patch->GetADCAmp() > fOnlineThresholds[trigger]) result.emplace_back(patch);
  }
  return result;
}

AliAnalysisTaskEmcalRecalcPatchesRef *AliAnalysisTaskEmcalRecalcPatchesRef::AddTaskEmcalRecalcPatches(const char *suffix) {
  AliAnalysisManager *mgr = AliAnalysisManager::GetAnalysisManager();
  if(!mgr){
    std::cerr << "No analysis manager defined" << std::endl;
    return nullptr;
  }

  std::stringstream taskname;
  taskname << "EmcalRecalcPatches_" << suffix;
  AliAnalysisTaskEmcalRecalcPatchesRef *task = new AliAnalysisTaskEmcalRecalcPatchesRef(taskname.str().data());
  mgr->AddTask(task);
  task->SetEnableSumw2(true);

  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdEG1, 115);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdEG2, 51);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdDG1, 115);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdDG2, 51);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdEJ1, 255);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdEJ2, 204);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdDJ1, 255);
  task->SetOnlineThreshold(AliAnalysisTaskEmcalRecalcPatchesRef::kThresholdDJ2, 204);

  std::stringstream outfilename, outlistname;
  outfilename << mgr->GetCommonFileName() << ":" << taskname.str();
  outlistname << "Histos_" << taskname.str();
  mgr->ConnectInput(task, 0, mgr->GetCommonInputContainer());
  mgr->ConnectOutput(task, 1, mgr->CreateContainer(outlistname.str().data(), TList::Class(), AliAnalysisManager::kOutputContainer, outfilename.str().data()));

  return task;
}