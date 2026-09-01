#include "mainwindow.h"
#include "core/appstate.h"
#include "core/project.h"
#include "engine/audioengine.h"
#include "engine/pluginmanager.h"
#include "engine/videoengine.h"

#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QInputDialog>
#include <QLineEdit>
#include <QTreeWidgetItemIterator>
#include <QFrame>
#include <QSignalBlocker>

void MainWindow::createDocks() {
    QDockWidget* sidebarDock = new QDockWidget("Library", this);
    sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sidebarDock->setTitleBarWidget(new QWidget());

    QWidget* sidebarWidget = new QWidget(sidebarDock);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(2, 2, 2, 2);
    sidebarLayout->setSpacing(2);

    QTabWidget* sidebarTabs = new QTabWidget(sidebarWidget);
    sidebarTabs->setTabPosition(QTabWidget::North);

    QWidget* projectTab = new QWidget(sidebarTabs);
    QVBoxLayout* projectLayout = new QVBoxLayout(projectTab);
    projectLayout->setContentsMargins(4, 4, 4, 4);
    projectLayout->setSpacing(6);

    mediaPool = new MediaPool(projectTab);
    connect(mediaPool, &MediaPool::mediaSelected, this, &MainWindow::onClipSelected);
    connect(mediaPool, &MediaPool::fileDropped, this, [this](const QString& filePath) {
        importMediaFile(filePath);
    });
    connect(mediaPool, &MediaPool::importRequested, this, &MainWindow::importVideo);
    projectLayout->addWidget(mediaPool, 1);

    trackControl = new TrackControl(projectTab);
    connect(trackControl, &TrackControl::trackSelected, this, [this](int row) { selectTrackIndex(row); });
    auto addTrack = [this](TimelineTrackType type) {
        TimelineTrack track;
        track.id = static_cast<int>(Project::instance().getTracks().size()) + 1;
        track.name = (type == TimelineTrackType::Audio ? "Audio " : "Video ") + std::to_string(track.id);
        track.type = type;
        Project::instance().getTracks().push_back(track);
        refreshTrackList();
        selectTrackIndex(static_cast<int>(Project::instance().getTracks().size()) - 1);
    };
    connect(trackControl, &TrackControl::newVideoTrackRequested, this, [addTrack]() { addTrack(TimelineTrackType::Video); });
    connect(trackControl, &TrackControl::newAudioTrackRequested, this, [addTrack]() { addTrack(TimelineTrackType::Audio); });
    connect(trackControl, &TrackControl::moveUpRequested, this, [this]() { moveSelectedTrack(-1); });
    connect(trackControl, &TrackControl::moveDownRequested, this, [this]() { moveSelectedTrack(1); });
    connect(trackControl, &TrackControl::deleteTrackRequested, this, [this]() { deleteSelectedTrack(); });
    connect(trackControl, &TrackControl::cutClipRequested, this, [this]() { cutClipAtPlayhead(); });
    connect(trackControl, &TrackControl::deleteClipRequested, this, [this]() { deleteSelectedClip(); });
    projectLayout->addWidget(trackControl, 1);

    sidebarTabs->addTab(projectTab, "Project");

    effectsBrowser = new EffectsBrowser(sidebarTabs);
    connect(effectsBrowser, &EffectsBrowser::effectDoubleClicked, this, &MainWindow::onEffectSelected);
    sidebarTabs->addTab(effectsBrowser, "Effects");

    QWidget* activeContainer = new QWidget(sidebarTabs);
    activeContainer->setObjectName("activeContainer");
    QVBoxLayout* activeLayout = new QVBoxLayout(activeContainer);
    activeLayout->setContentsMargins(8, 8, 8, 8);
    QLabel* activeTitle = new QLabel("ACTIVE EFFECTS", activeContainer);
    activeTitle->setStyleSheet("font-weight: bold; color: #FF72AA; font-size: 11px; letter-spacing: 0.5px;");
    activeLayout->addWidget(activeTitle);
    activeEffectsList = new QListWidget(activeContainer);
    connect(activeEffectsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onActiveEffectSelected);
    activeLayout->addWidget(activeEffectsList, 1);

    QPushButton* removeEffectButton = new QPushButton("Remove Selected Effect", activeContainer);
    removeEffectButton->setStyleSheet("QPushButton { background: #32101F; color: #FFB8D2; border: 1px solid #80324F; padding: 6px; font-size: 11px; font-weight: bold; border-radius: 3px; } QPushButton:hover { background: #5C1E38; color: white; border-color: #FF4F91; }");
    connect(removeEffectButton, &QPushButton::clicked, this, &MainWindow::removeSelectedEffect);
    activeLayout->addWidget(removeEffectButton);

    sidebarTabs->addTab(activeContainer, "Active FX");

    auto* detectScroll = new QScrollArea(sidebarTabs);
    detectScroll->setWidgetResizable(true);
    detectScroll->setFrameShape(QFrame::NoFrame);
    QWidget* detectTab = new QWidget(detectScroll);
    QVBoxLayout* detectLayout = new QVBoxLayout(detectTab);
    detectLayout->setContentsMargins(8, 8, 8, 8);
    detectLayout->setSpacing(8);

    const auto addSection = [detectTab, detectLayout](const QString& title) {
        auto* section = new QLabel(title, detectTab);
        section->setStyleSheet("font-weight: bold; color: #FF72AA; font-size: 10px; letter-spacing: 0.6px; padding-top: 6px;");
        detectLayout->addWidget(section);
    };
    const auto addSlider = [detectTab, detectLayout](const QString& title, int minimum, int maximum,
                                                       int initial, const QString& suffix = QString()) {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel(title, detectTab);
        label->setStyleSheet("color: #C3BEC3; font-size: 10px;");
        row->addWidget(label);
        auto* slider = new QSlider(Qt::Horizontal, detectTab);
        slider->setRange(minimum, maximum);
        slider->setValue(initial);
        row->addWidget(slider, 1);
        auto* valueLabel = new QLabel(QString::number(initial) + suffix, detectTab);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setMinimumWidth(34);
        valueLabel->setStyleSheet("color: #FFB8D2; font-family: 'JetBrains Mono', monospace; font-size: 10px;");
        row->addWidget(valueLabel);
        connect(slider, &QSlider::valueChanged, detectTab, [valueLabel, suffix](int value) {
            valueLabel->setText(QString::number(value) + suffix);
        });
        detectLayout->addLayout(row);
        return slider;
    };
    const auto addCombo = [detectTab, detectLayout](const QString& title, QComboBox* combo) {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel(title, detectTab);
        label->setStyleSheet("color: #C3BEC3; font-size: 10px;");
        row->addWidget(label);
        row->addWidget(combo, 1);
        detectLayout->addLayout(row);
    };

    QLabel* detectTitle = new QLabel("DETECT & MASK", detectTab);
    detectTitle->setStyleSheet("font-weight: bold; color: #FF72AA; font-size: 11px; letter-spacing: 0.5px;");
    detectLayout->addWidget(detectTitle);

    QLabel* detectHint = new QLabel("Recommended: the official YOLOv5x6 COCO model (about 282 MB), verified in Z at 640 px with its OpenCV backend. It recognizes 80 COCO classes; choose the classes and reviewed tracks that remain active. All model work, clip scanning, and mask building run outside the UI thread.", detectTab);
    detectHint->setWordWrap(true);
    detectHint->setStyleSheet("color: #918B92; font-size: 10px;");
    detectLayout->addWidget(detectHint);

    QHBoxLayout* modelLayout = new QHBoxLayout();
    yoloModelPathEdit = new QLineEdit(detectTab);
    yoloModelPathEdit->setPlaceholderText("YOLO .onnx model (optional)");
    modelLayout->addWidget(yoloModelPathEdit, 1);
    QPushButton* modelButton = new QPushButton("Load YOLO", detectTab);
    connect(modelButton, &QPushButton::clicked, this, &MainWindow::chooseYoloModel);
    modelLayout->addWidget(modelButton);
    detectLayout->addLayout(modelLayout);

    QPushButton* downloadModelButton = new QPushButton("Download Recommended YOLOv5x6 Model (282 MB)", detectTab);
    downloadModelButton->setToolTip("Downloads the official highest-capacity YOLOv5 COCO ONNX model, validated at 640 px with Z's OpenCV DNN backend. Loading and inference stay on the background detection worker.");
    connect(downloadModelButton, &QPushButton::clicked, this, &MainWindow::downloadRecommendedYoloModel);
    detectLayout->addWidget(downloadModelButton);

    addSection("INFERENCE");
    openClCheck = new QCheckBox("Prefer OpenCL acceleration", detectTab);
    openClCheck->setChecked(true);
    connect(openClCheck, &QCheckBox::toggled, this, [this](bool on) {
        detectionWorkerSettings.preferOpenCL = on;
        scheduleDetectionSettingsRefresh();
    });
    detectLayout->addWidget(openClCheck);

    yoloConfidenceSlider = addSlider("YOLO confidence", 10, 90, 25, "%");
    connect(yoloConfidenceSlider, &QSlider::valueChanged, this, [this](int value) {
        detectionWorkerSettings.yoloConfidence = value / 100.0f;
        scheduleDetectionSettingsRefresh();
    });

    yoloNmsSlider = addSlider("YOLO overlap / NMS", 10, 90, 45, "%");
    connect(yoloNmsSlider, &QSlider::valueChanged, this, [this](int value) {
        detectionWorkerSettings.yoloNmsThreshold = value / 100.0f;
        scheduleDetectionSettingsRefresh();
    });

    yoloInputSizeCombo = new QComboBox(detectTab);
    yoloInputSizeCombo->addItem("Model-safe automatic (640 px)", 0);
    yoloInputSizeCombo->addItem("640 px — verified YOLOv5x6", 640);
    yoloInputSizeCombo->addItem("960 px — experimental / custom model", 960);
    yoloInputSizeCombo->addItem("1280 px — experimental; incompatible with YOLOv5x6", 1280);
    yoloInputSizeCombo->setToolTip("YOLOv5x6 is verified at 640 px in the bundled OpenCV DNN path. Higher resolutions are for compatible custom models; 1280 px is known to fail for YOLOv5x6 with this OpenCV build.");
    addCombo("Inference resolution", yoloInputSizeCombo);
    connect(yoloInputSizeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        detectionWorkerSettings.yoloInputSize = yoloInputSizeCombo->currentData().toInt();
        scheduleDetectionSettingsRefresh();
    });

    detectionStatusLabel = new QLabel("Motion region fallback", detectTab);
    detectionStatusLabel->setWordWrap(true);
    detectionStatusLabel->setStyleSheet("color: #918B92; font-size: 10px;");
    detectLayout->addWidget(detectionStatusLabel);

    QPushButton* runDetectBtn = new QPushButton("Detect Current Frame", detectTab);
    runDetectBtn->setStyleSheet("QPushButton { background: #32101F; color: #FFB8D2; border: 1px solid #80324F; padding: 6px; font-weight: bold; border-radius: 3px; } QPushButton:hover { background: #5C1E38; border-color: #FF4F91; }");
    connect(runDetectBtn, &QPushButton::clicked, this, &MainWindow::runDetectionOnCurrentFrame);
    detectLayout->addWidget(runDetectBtn);

    addSection("COCO CLASS FILTER");
    QLabel* classHint = new QLabel("Search and uncheck any classes you do not want. This re-filters completed results immediately; it does not rerun the model.", detectTab);
    classHint->setWordWrap(true);
    classHint->setStyleSheet("color: #918B92; font-size: 10px;");
    detectLayout->addWidget(classHint);
    detectionClassFilterEdit = new QLineEdit(detectTab);
    detectionClassFilterEdit->setPlaceholderText("Filter classes…");
    detectLayout->addWidget(detectionClassFilterEdit);
    detectionClassList = new QListWidget(detectTab);
    detectionClassList->setMaximumHeight(200);
    detectionClassList->setStyleSheet("QListWidget { background: #08080A; border: 1px solid #303036; } QListWidget::item { padding: 2px 4px; }");
    for (const auto& className : Detector::cocoClassLabels()) {
        auto* item = new QListWidgetItem(QString::fromStdString(className), detectionClassList);
        item->setData(Qt::UserRole, QString::fromStdString(className));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    connect(detectionClassFilterEdit, &QLineEdit::textChanged, this, [this](const QString& filter) {
        if (!detectionClassList) return;
        for (int row = 0; row < detectionClassList->count(); ++row) {
            auto* item = detectionClassList->item(row);
            item->setHidden(!item->text().contains(filter, Qt::CaseInsensitive));
        }
    });
    connect(detectionClassList, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        updateClassFilterFromUi();
    });
    detectLayout->addWidget(detectionClassList);
    auto* classButtons = new QHBoxLayout();
    auto* allClassesButton = new QPushButton("Select All", detectTab);
    auto* noClassesButton = new QPushButton("Select None", detectTab);
    classButtons->addWidget(allClassesButton);
    classButtons->addWidget(noClassesButton);
    connect(allClassesButton, &QPushButton::clicked, this, [this] {
        if (!detectionClassList) return;
        const QSignalBlocker blocker(detectionClassList);
        for (int row = 0; row < detectionClassList->count(); ++row) {
            detectionClassList->item(row)->setCheckState(Qt::Checked);
        }
        updateClassFilterFromUi();
    });
    connect(noClassesButton, &QPushButton::clicked, this, [this] {
        if (!detectionClassList) return;
        const QSignalBlocker blocker(detectionClassList);
        for (int row = 0; row < detectionClassList->count(); ++row) {
            detectionClassList->item(row)->setCheckState(Qt::Unchecked);
        }
        updateClassFilterFromUi();
    });
    detectLayout->addLayout(classButtons);

    addSection("WHOLE-CLIP REVIEW");
    QLabel* scanHint = new QLabel("Pre-detect a clip on the background worker, then uncheck a stable track below to remove it from the preview and effect mask.", detectTab);
    scanHint->setWordWrap(true);
    scanHint->setStyleSheet("color: #918B92; font-size: 10px;");
    detectLayout->addWidget(scanHint);
    detectionScanIntervalSlider = addSlider("Scan sample interval", 250, 10000, 1000, " ms");
    detectEntireClipButton = new QPushButton("Detect Entire Active Clip", detectTab);
    detectEntireClipButton->setStyleSheet("QPushButton { background: #32101F; color: #FFB8D2; border: 1px solid #80324F; padding: 6px; font-weight: bold; border-radius: 3px; } QPushButton:hover { background: #5C1E38; border-color: #FF4F91; }");
    connect(detectEntireClipButton, &QPushButton::clicked, this, &MainWindow::detectEntireActiveClip);
    detectLayout->addWidget(detectEntireClipButton);
    cancelDetectionScanButton = new QPushButton("Cancel Whole-Clip Detection", detectTab);
    cancelDetectionScanButton->setEnabled(false);
    connect(cancelDetectionScanButton, &QPushButton::clicked, this, &MainWindow::cancelDetectionScan);
    detectLayout->addWidget(cancelDetectionScanButton);

    addSection("TRACKING");
    liveDetectCheck = new QCheckBox("Live detection during playback", detectTab);
    liveDetectCheck->setStyleSheet("color: #C3BEC3;");
    connect(liveDetectCheck, &QCheckBox::toggled, this, [this](bool on) {
        liveDetectEnabled = on;
        liveDetectionTimer.restart();
        if (on) runDetectionOnCurrentFrame();
    });
    detectLayout->addWidget(liveDetectCheck);

    liveDetectionIntervalSlider = addSlider("Detection interval", 250, 2000, 750, " ms");

    QLabel* fallbackHint = new QLabel("Fallback motion-region controls", detectTab);
    fallbackHint->setStyleSheet("color: #918B92; font-size: 10px; padding-top: 3px;");
    detectLayout->addWidget(fallbackHint);
    detectSensitivitySlider = addSlider("Sensitivity", 5, 95, 45, "%");
    connect(detectSensitivitySlider, &QSlider::valueChanged, this, &MainWindow::onDetectionSettingsChanged);
    detectMinAreaSlider = addSlider("Minimum region area", 1, 40, 12, " ‰");
    connect(detectMinAreaSlider, &QSlider::valueChanged, this, &MainWindow::onDetectionSettingsChanged);

    addSection("OVERLAY & TRACERS");
    showBoxesCheck = new QCheckBox("Show detection overlay", detectTab);
    showBoxesCheck->setChecked(true);
    showBoxesCheck->setStyleSheet("color: #C3BEC3;");
    connect(showBoxesCheck, &QCheckBox::toggled, this, [this](bool) {
        applyDetectionOverlayOptions();
    });
    detectLayout->addWidget(showBoxesCheck);

    showDetectionLabelsCheck = new QCheckBox("Show object names", detectTab);
    showDetectionLabelsCheck->setChecked(true);
    connect(showDetectionLabelsCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showDetectionLabelsCheck);
    showDetectionConfidenceCheck = new QCheckBox("Show confidence", detectTab);
    showDetectionConfidenceCheck->setChecked(true);
    connect(showDetectionConfidenceCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showDetectionConfidenceCheck);
    showDetectionTrackIdsCheck = new QCheckBox("Show stable track IDs", detectTab);
    connect(showDetectionTrackIdsCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showDetectionTrackIdsCheck);
    showDetectionTrailsCheck = new QCheckBox("Show motion tracers", detectTab);
    showDetectionTrailsCheck->setChecked(true);
    connect(showDetectionTrailsCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showDetectionTrailsCheck);
    showDetectionLinksCheck = new QCheckBox("Link nearby tracked objects", detectTab);
    connect(showDetectionLinksCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showDetectionLinksCheck);
    showDetectionCentersCheck = new QCheckBox("Show object centres", detectTab);
    connect(showDetectionCentersCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showDetectionCentersCheck);
    showPersonOutlineCheck = new QCheckBox("Show person silhouette guide", detectTab);
    showPersonOutlineCheck->setChecked(true);
    showPersonOutlineCheck->setToolTip("A clear human-shaped guide derived from the detected person box. It is not a semantic segmentation contour.");
    connect(showPersonOutlineCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(showPersonOutlineCheck);
    replacePersonBoxesCheck = new QCheckBox("Use guide instead of person box", detectTab);
    replacePersonBoxesCheck->setChecked(true);
    connect(replacePersonBoxesCheck, &QCheckBox::toggled, this, [this](bool) { applyDetectionOverlayOptions(); });
    detectLayout->addWidget(replacePersonBoxesCheck);

    detectionOverlayStyleCombo = new QComboBox(detectTab);
    detectionOverlayStyleCombo->addItem("Corner brackets", static_cast<int>(DetectionOverlayStyle::CornerBrackets));
    detectionOverlayStyleCombo->addItem("Rectangle", static_cast<int>(DetectionOverlayStyle::Rectangle));
    detectionOverlayStyleCombo->addItem("Rounded rectangle", static_cast<int>(DetectionOverlayStyle::RoundedRectangle));
    detectionOverlayStyleCombo->addItem("Ellipse", static_cast<int>(DetectionOverlayStyle::Ellipse));
    addCombo("Box style", detectionOverlayStyleCombo);
    connect(detectionOverlayStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { applyDetectionOverlayOptions(); });

    detectionColorModeCombo = new QComboBox(detectTab);
    detectionColorModeCombo->addItem("Stable track colour", static_cast<int>(DetectionColorMode::ByTrack));
    detectionColorModeCombo->addItem("Class colour", static_cast<int>(DetectionColorMode::ByClass));
    detectionColorModeCombo->addItem("Fixed magenta", static_cast<int>(DetectionColorMode::Fixed));
    addCombo("Colour mode", detectionColorModeCombo);
    connect(detectionColorModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { applyDetectionOverlayOptions(); });

    detectionLineWidthSlider = addSlider("Box line width", 1, 8, 2, " px");
    connect(detectionLineWidthSlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });
    detectionFillOpacitySlider = addSlider("Box fill", 0, 180, 0);
    connect(detectionFillOpacitySlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });
    detectionLabelSizeSlider = addSlider("Name label size", 10, 36, 15, " pt");
    connect(detectionLabelSizeSlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });
    detectionTrailLengthSlider = addSlider("Tracer history", 2, 120, 30, " points");
    connect(detectionTrailLengthSlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });
    detectionTrailWidthSlider = addSlider("Tracer width", 1, 8, 2, " px");
    connect(detectionTrailWidthSlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });
    detectionTrailOpacitySlider = addSlider("Tracer opacity", 0, 255, 180);
    connect(detectionTrailOpacitySlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });
    detectionLinkDistanceSlider = addSlider("Link distance", 5, 100, 25, "%");
    connect(detectionLinkDistanceSlider, &QSlider::valueChanged, this, [this](int) { applyDetectionOverlayOptions(); });

    addSection("EFFECT MASK");
    detectionShapeCombo = new QComboBox(detectTab);
    detectionShapeCombo->addItem("Rectangle", static_cast<int>(DetectionShape::Rectangle));
    detectionShapeCombo->addItem("Ellipse", static_cast<int>(DetectionShape::Ellipse));
    detectionShapeCombo->addItem("Outline", static_cast<int>(DetectionShape::Outline));
    detectionShapeCombo->addItem("Person silhouette guide", static_cast<int>(DetectionShape::PersonOutline));
    detectionShapeCombo->setToolTip("Choose the background-generated shape that limits active effects. Person silhouette uses the box-derived guide when available.");
    addCombo("Mask shape", detectionShapeCombo);
    connect(detectionShapeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshDetectionMask();
    });

    applyMaskCheck = new QCheckBox("Mask all active effects", detectTab);
    applyMaskCheck->setStyleSheet("color: #C3BEC3;");
    connect(applyMaskCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (on && currentDetections.empty()) runDetectionOnCurrentFrame();
        refreshDetectionMask();
    });
    detectLayout->addWidget(applyMaskCheck);

    invertMaskCheck = new QCheckBox("Invert mask (affect background)", detectTab);
    connect(invertMaskCheck, &QCheckBox::toggled, this, [this](bool) { refreshDetectionMask(); });
    detectLayout->addWidget(invertMaskCheck);
    maskFeatherSlider = addSlider("Mask feather", 0, 80, 6, " px");
    connect(maskFeatherSlider, &QSlider::valueChanged, this, [this](int) { refreshDetectionMask(); });
    maskPaddingSlider = addSlider("Mask padding", 0, 160, 0, " px");
    connect(maskPaddingSlider, &QSlider::valueChanged, this, [this](int) { refreshDetectionMask(); });
    maskOutlineWidthSlider = addSlider("Outline mask width", 1, 80, 6, " px");
    connect(maskOutlineWidthSlider, &QSlider::valueChanged, this, [this](int) { refreshDetectionMask(); });

    addSection("CURRENT OBJECTS & TRACK REVIEW");
    detectionList = new QListWidget(detectTab);
    detectionList->setStyleSheet("QListWidget { background: #08080A; border: 1px solid #303036; }");
    connect(detectionList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (!item || detectionSourceClipId.isEmpty()) return;
        bool validTrack = false;
        const int trackId = item->data(Qt::UserRole).toInt(&validTrack);
        if (!validTrack || trackId <= 0) return;
        auto& rejected = rejectedDetectionTracks[detectionSourceClipId.toStdString()];
        if (item->checkState() == Qt::Checked) {
            rejected.erase(trackId);
        } else {
            rejected.insert(trackId);
        }
        applyCurrentDetectionFilters();
    });
    detectLayout->addWidget(detectionList, 1);

    auto* reviewButtons = new QHBoxLayout();
    auto* keepAllButton = new QPushButton("Keep All", detectTab);
    auto* rejectAllButton = new QPushButton("Reject All", detectTab);
    reviewButtons->addWidget(keepAllButton);
    reviewButtons->addWidget(rejectAllButton);
    connect(keepAllButton, &QPushButton::clicked, this, [this] {
        if (detectionSourceClipId.isEmpty()) return;
        rejectedDetectionTracks[detectionSourceClipId.toStdString()].clear();
        applyCurrentDetectionFilters();
    });
    connect(rejectAllButton, &QPushButton::clicked, this, [this] {
        if (detectionSourceClipId.isEmpty()) return;
        auto& rejected = rejectedDetectionTracks[detectionSourceClipId.toStdString()];
        for (const auto& box : rawCurrentDetections) {
            if (box.trackId > 0) rejected.insert(box.trackId);
        }
        applyCurrentDetectionFilters();
    });
    detectLayout->addLayout(reviewButtons);

    QPushButton* clearDetectBtn = new QPushButton("Clear Detections", detectTab);
    connect(clearDetectBtn, &QPushButton::clicked, this, &MainWindow::clearDetections);
    detectLayout->addWidget(clearDetectBtn);

    detectLayout->addStretch();
    detectScroll->setWidget(detectTab);
    sidebarTabs->addTab(detectScroll, "Detect");
    applyDetectionOverlayOptions();

    sidebarLayout->addWidget(sidebarTabs);
    sidebarWidget->setLayout(sidebarLayout);
    sidebarDock->setWidget(sidebarWidget);
    addDockWidget(Qt::LeftDockWidgetArea, sidebarDock);

    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setTitleBarWidget(new QWidget());

    QWidget* controlContainer = new QWidget(inspectorDock);
    controlContainer->setObjectName("controlContainer");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlContainer);
    controlLayout->setContentsMargins(8, 8, 8, 8);
    QLabel* controlTitle = new QLabel("INSPECTOR", controlContainer);
    controlTitle->setStyleSheet("font-weight: bold; color: #FF72AA; font-size: 11px; letter-spacing: 0.5px;");
    controlLayout->addWidget(controlTitle);

    inspectorPanel = new Inspector(controlContainer);
    inspectorPanel->setStyleSheet("background: transparent; border: none;");
    controlLayout->addWidget(inspectorPanel, 1);
    connect(inspectorPanel, &Inspector::parameterChanged, this, &MainWindow::onParameterChanged);
    connect(inspectorPanel, &Inspector::scrubRequested, this, &MainWindow::onTimelineScrubbed);
    connect(inspectorPanel, &Inspector::keyframeRemoveRequested, this, [this](const QString& effId, const QString& paramName, double time) {
        auto* effects = activeEffects();
        if (!effects) return;
        for (auto& eff : *effects) {
            if (QString::fromStdString(eff.pluginId) == effId) {
                for (auto& param : eff.parameters) {
                    if (QString::fromStdString(param.name) == paramName) {
                        param.curve.removeKeyframeAt(time, 0.05);
                        if (timelinePanel) timelinePanel->update();
                        if (inspectorPanel) inspectorPanel->syncParameters(eff.parameters);
                        onTimelineScrubbed(currentPlayhead);
                        return;
                    }
                }
            }
        }
    });
    connect(inspectorPanel, &Inspector::keyframeRequested, this, [this](const QString& effId, const QString& paramName, double time, double value) {
        auto* effects = activeEffects();
        if (!effects) return;
        for (auto& eff : *effects) {
            if (QString::fromStdString(eff.pluginId) == effId) {
                for (auto& param : eff.parameters) {
                    if (QString::fromStdString(param.name) == paramName) {
                        param.curve.insertKeyframe(time, value);
                        if (timelinePanel) timelinePanel->update();
                        if (inspectorPanel) inspectorPanel->syncParameters(eff.parameters);
                        onTimelineScrubbed(currentPlayhead);
                        return;
                    }
                }
            }
        }
    });
    connect(inspectorPanel, &Inspector::keyframeInterpolationRequested, this, [this](const QString& effId, const QString& paramName, double time, int mode) {
        auto* effects = activeEffects();
        if (!effects) return;
        for (auto& effect : *effects) {
            if (QString::fromStdString(effect.pluginId) != effId) continue;
            for (auto& parameter : effect.parameters) {
                if (QString::fromStdString(parameter.name) == paramName &&
                    parameter.curve.setInterpolationAt(time, static_cast<InterpolationMode>(mode))) {
                    if (timelinePanel) timelinePanel->update();
                    if (inspectorPanel) inspectorPanel->syncParameters(effect.parameters);
                    onTimelineScrubbed(currentPlayhead);
                    return;
                }
            }
        }
    });

    connect(inspectorPanel, &Inspector::removeEffectRequested, this, [this](const QString& effectId) {
        auto* effects = activeEffects();
        if (!effects) return;
        auto it = std::remove_if(effects->begin(), effects->end(), [&](const AppliedEffect& eff) {
            return QString::fromStdString(eff.pluginId) == effectId;
        });
        if (it != effects->end()) {
            AppState::instance().pushUndoState();
            effects->erase(it, effects->end());
            inspectorPanel->clearInspector();
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
            timelinePanel->update();
            onTimelineScrubbed(currentPlayhead);
        }
    });

    connect(inspectorPanel, &Inspector::parameterSelected, this, [this](const QString& effId, const QString& paramName) {
        timelinePanel->selectParameter(effId, paramName);
    });

    inspectorDock->setWidget(controlContainer);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    QDockWidget* bottomDock = new QDockWidget("Workspaces", this);
    bottomDock->setTitleBarWidget(new QWidget());
    QWidget* bottomContainer = new QWidget(bottomDock);
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(4, 4, 4, 4);
    bottomLayout->setSpacing(4);

    bottomTabs = new QTabWidget(bottomContainer);
    bottomTabs->setTabPosition(QTabWidget::South);

    timelinePanel = new Timeline(this);
    connect(timelinePanel, &Timeline::scrubbed, this, &MainWindow::onTimelineScrubbed);
    connect(timelinePanel, &Timeline::transitionApplyRequested, this, [this](int trackIndex, double cutTime, const QString& transitionId) {
        if (addTransitionAtCut(trackIndex, cutTime, transitionId)) {
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
        }
    });
    connect(timelinePanel, &Timeline::clipSelected, this, [this](int trackIndex, int clipIndex) {
        selectTrackIndex(trackIndex);
        selectClip(trackIndex, clipIndex);
    });
    connect(timelinePanel, &Timeline::fileDropped, this, [this](int trackIndex, double dropTime, const QString& filePath) {
        importMediaFile(filePath, trackIndex, dropTime);
    });
    connect(timelinePanel, &Timeline::effectDropped, this, [this](int trackIndex, double dropTime, const QString& effectName) {
        QString pluginId;
        bool isTransition = false;
        if (effectsBrowser) {
            auto* tree = effectsBrowser->getTreeWidget();
            QTreeWidgetItemIterator it(tree);
            while (*it) {
                if ((*it)->text(0) == effectName || (*it)->data(0, Qt::UserRole).toString() == effectName) {
                    pluginId = (*it)->data(0, Qt::UserRole).toString();
                    QTreeWidgetItem* p = (*it)->parent();
                    while (p) {
                        if (p->text(0) == "Transitions") {
                            isTransition = true;
                            break;
                        }
                        p = p->parent();
                    }
                    break;
                }
                ++it;
            }
        }
        if (pluginId.isEmpty()) return;

        if (isTransition) {
            const auto& tracks = Project::instance().getTracks();
            if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size()) || tracks[trackIndex].type == TimelineTrackType::Audio) return;
            if (addTransitionAtCut(trackIndex, dropTime, pluginId)) {
                refreshActiveEffectsList();
                syncEffectStackToRenderer();
            }
        } else {
            auto& tracks = Project::instance().getTracks();
            if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
            auto& track = tracks[trackIndex];
            if (track.type == TimelineTrackType::Audio) return;
            ProjectClip* targetClip = nullptr;
            for (auto& c : track.clips) {
                if (dropTime >= c.timelineStart && dropTime < c.timelineStart + c.sourceDuration) {
                    targetClip = &c;
                    break;
                }
            }
            if (!targetClip) return;

            AppState::instance().pushUndoState();
            if (!targetClip->useClipEffects) {
                targetClip->effects = track.effects;
                targetClip->useClipEffects = true;
            }
            targetClip->effects.push_back(createEffectTemplate(pluginId));
            selectedClipId = QString::fromStdString(targetClip->id);
            timelinePanel->update();
            refreshActiveEffectsList();
            syncEffectStackToRenderer();
        }
    });
    connect(timelinePanel, &Timeline::clipMoveStarted, this, [this]() {
        AppState::instance().pushUndoState();
    });
    connect(timelinePanel, &Timeline::clipMoveFinished, this, [this]() {
        auto& tracks = Project::instance().getTracks();
        for (auto& track : tracks) {
            sortTrackClips(track);
        }
        refreshTrackList();
        timelinePanel->update();
    });
    connect(timelinePanel, &Timeline::clipMoveRequested, this, [this](int trackIndex, int clipIndex, double newTimelineStart) {
        auto& tracks = Project::instance().getTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
        auto& track = tracks[trackIndex];
        if (clipIndex < 0 || clipIndex >= static_cast<int>(track.clips.size())) return;

        auto& clip = track.clips[clipIndex];
        clip.timelineStart = std::max(0.0, newTimelineStart);
        activeAudioClipId.clear();
        timelinePanel->setDuration(std::max(10.0, Project::instance().getDuration() + 5.0));
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    });
    connect(timelinePanel, &Timeline::clipTrackChangeRequested, this, [this](int fromTrack, int fromClip, int toTrack, double newTimelineStart) {
        auto& tracks = Project::instance().getTracks();
        if (fromTrack < 0 || fromTrack >= static_cast<int>(tracks.size())) return;
        if (toTrack < 0 || toTrack > static_cast<int>(tracks.size())) return;

        if (toTrack == static_cast<int>(tracks.size())) {
            TimelineTrack newT;
            newT.id = tracks.size() + 1;
            newT.type = tracks[fromTrack].type;
            newT.name = newT.type == TimelineTrackType::Audio ? "Audio " + std::to_string(newT.id) : "Video " + std::to_string(newT.id);
            tracks.push_back(newT);
        }
        auto& srcTrack = tracks[fromTrack];
        if (fromClip < 0 || fromClip >= static_cast<int>(srcTrack.clips.size())) return;
        if (tracks[toTrack].type != srcTrack.type) return;
        auto clipCopy = srcTrack.clips[fromClip];
        clipCopy.timelineStart = std::max(0.0, newTimelineStart);
        srcTrack.clips.erase(srcTrack.clips.begin() + fromClip);
        auto& dstTrack = tracks[toTrack];
        dstTrack.clips.push_back(clipCopy);
        activeAudioClipId.clear();
        sortTrackClips(srcTrack);
        sortTrackClips(dstTrack);
        int newClipIndex = -1;
        for (int i = 0; i < static_cast<int>(dstTrack.clips.size()); ++i) {
            if (dstTrack.clips[i].id == clipCopy.id) {
                newClipIndex = i;
                break;
            }
        }
        if (newClipIndex != -1) timelinePanel->updateDragIndices(toTrack, newClipIndex);
        timelinePanel->setDuration(std::max(10.0, Project::instance().getDuration() + 5.0));
        refreshTrackList();
        timelinePanel->update();
        onTimelineScrubbed(currentPlayhead);
    });

    connect(timelinePanel, &Timeline::deleteClipRequested, this, [this](int trackIndex, int clipIndex) {
        AppState::instance().pushUndoState();
        auto& tracks = Project::instance().getTracks();
        if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size())) {
            if (clipIndex >= 0 && clipIndex < static_cast<int>(tracks[trackIndex].clips.size())) {
                const std::string removedId = tracks[trackIndex].clips[clipIndex].id;
                tracks[trackIndex].clips.erase(tracks[trackIndex].clips.begin() + clipIndex);
                if (selectedClipId == QString::fromStdString(removedId)) {
                    selectedClipId.clear();
                }
                refreshTrackList();
                timelinePanel->update();
            }
        }
    });

    connect(timelinePanel, &Timeline::renameClipRequested, this, [this](int trackIndex, int clipIndex) {
        auto& tracks = Project::instance().getTracks();
        if (trackIndex >= 0 && trackIndex < static_cast<int>(tracks.size())) {
            if (clipIndex >= 0 && clipIndex < static_cast<int>(tracks[trackIndex].clips.size())) {
                QString currentName = QString::fromStdString(tracks[trackIndex].clips[clipIndex].name);
                bool ok;
                QString newName = QInputDialog::getText(this, "Rename Clip", "New name:", QLineEdit::Normal, currentName, &ok);
                if (ok && !newName.isEmpty()) {
                    AppState::instance().pushUndoState();
                    tracks[trackIndex].clips[clipIndex].name = newName.toStdString();
                    timelinePanel->update();
                }
            }
        }
    });

    connect(timelinePanel, &Timeline::deleteTransitionRequested, this, [this](int trackIndex, int transIndex) {
        AppState::instance().pushUndoState();
        auto& tracks = Project::instance().getTracks();
        if (trackIndex >= 0 && trackIndex < (int)tracks.size()) {
            auto& transitions = tracks[trackIndex].transitions;
            if (transIndex >= 0 && transIndex < (int)transitions.size()) {
                transitions.erase(transitions.begin() + transIndex);
                timelinePanel->update();
            }
        }
    });

    QScrollArea* timelineScroll = new QScrollArea(this);
    timelineScroll->setWidget(timelinePanel);
    timelineScroll->setWidgetResizable(true);
    timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    timelineScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    bottomTabs->addTab(timelineScroll, "Timeline Editor");

    bottomLayout->addWidget(bottomTabs);
    bottomDock->setWidget(bottomContainer);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);

    sidebarDock->setMinimumWidth(220);
    inspectorDock->setMinimumWidth(260);

    QList<QDockWidget*> hDocks;
    hDocks << sidebarDock << inspectorDock;
    QList<int> hSizes;
    hSizes << 240 << 300;
    resizeDocks(hDocks, hSizes, Qt::Horizontal);

    bottomDock->setMinimumHeight(240);
}
