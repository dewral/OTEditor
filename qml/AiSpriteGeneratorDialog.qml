import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: dialog
    objectName: "aiSpriteGeneratorDialog"
    property var service
    signal slicerRequested(url imageUrl)
    title: "AI Sprite Generator"
    anchors.centerIn: parent
    width: Math.min(940, parent.width - 32)
    height: Math.min(720, parent.height - 32)
    modal: true
    standardButtons: Dialog.NoButton
    readonly property bool hasImage: service.imageUrl.toString().length > 0
    readonly property string chosenModelId: modelPicker.currentIndex === 2 ? modelId.text.trim() : modelPicker.currentValue
    readonly property int promptLength: Array.from(prompt.text.trim()).length

    function applySettings() {
        return service.configure({ generationUrl: generationUrl.text.trim(),
            authHeader: authHeader.text.trim(), authPrefix: authPrefix.text,
            statusUrl: statusUrl.text.trim(), modelId: chosenModelId })
    }
    Connections {
        target: dialog.service
        function onSettingsChanged() {
            authHeader.text = dialog.service.settings.authHeader
            authPrefix.text = dialog.service.settings.authPrefix
        }
    }
    function statusText() {
        switch (service.state) {
        case "submitting": return "Submitting generation...";
        case "waiting": return "Generating... " + service.progress + "%";
        case "downloading": return "Downloading PNG...";
        case "completed": return "PNG ready.";
        case "cancelled": return "Stopped locally. The service may still finish and charge for the job.";
        case "error": return "Request failed.";
        default: return "Enter your API key and prompt to generate a sprite.";
        }
    }
    Component.onCompleted: {
        const saved = service.settings
        generationUrl.text = saved.generationUrl
        authHeader.text = saved.authHeader
        authPrefix.text = saved.authPrefix
        statusUrl.text = saved.statusUrl
        modelId.text = saved.modelId
        modelPicker.currentIndex = saved.modelId === "qwen21-midhem-256" ? 0 : saved.modelId === "tibia-style-items-1" ? 1 : 2
    }

    FileDialog {
        id: saveDialog
        title: "Save generated PNG"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]
        defaultSuffix: "png"
        onAccepted: dialog.service.savePng(selectedFile)
    }

    footer: Control {
        padding: 12
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: dialog.statusText(); wrapMode: Text.Wrap }
            Label {
                Layout.fillWidth: true
                visible: dialog.service.errorString.length > 0
                text: dialog.service.errorString
                wrapMode: Text.Wrap
                color: "#ff9b91"
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: dialog.service.busy
                from: 0; to: 100; value: dialog.service.progress
                indeterminate: dialog.service.state !== "waiting" || dialog.service.progress === 0
            }
            RowLayout {
                Button {
                    text: "Generate"
                    enabled: !dialog.service.busy && dialog.promptLength > 0 && dialog.promptLength <= 2000 && apiKey.text.length > 0 && dialog.chosenModelId.length > 0
                    onClicked: if (dialog.applySettings()) dialog.service.generate(prompt.text, dialog.chosenModelId, apiKey.text)
                }
                Button {
                    text: "Retry Submission"
                    visible: dialog.service.canRetrySubmission
                    enabled: apiKey.text.length > 0
                    onClicked: dialog.service.retrySubmission(apiKey.text)
                    ToolTip.visible: hovered
                    ToolTip.text: "Resend the original prompt and model with the same request key to avoid a second charge."
                }
                Button {
                    text: "Check Result"
                    enabled: !dialog.service.busy && dialog.service.jobId.length > 0 && apiKey.text.length > 0
                    onClicked: if (dialog.applySettings()) dialog.service.checkResult(apiKey.text)
                }
                Button { text: "Cancel"; enabled: dialog.service.busy; onClicked: dialog.service.cancel() }
                Item { Layout.fillWidth: true }
                Button { text: "Close"; onClicked: dialog.close() }
            }
        }
    }

    contentItem: RowLayout {
        spacing: 16
        ScrollView {
            Layout.preferredWidth: 400
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 10
                Label { text: "AI Sprite Studio"; font.bold: true }
                Label { text: "Model" }
                ComboBox {
                    id: modelPicker
                    Layout.fillWidth: true
                    enabled: !dialog.service.busy
                    textRole: "name"; valueRole: "modelId"
                    model: [
                        { name: "Creature Sheet · 12 credits · 256 × 192", modelId: "qwen21-midhem-256" },
                        { name: "Flat 2D Item · 4 credits · 32 × 32", modelId: "tibia-style-items-1" },
                        { name: "Custom model", modelId: "" }
                    ]
                }
                TextField { id: modelId; visible: modelPicker.currentIndex === 2; Layout.fillWidth: true; enabled: !dialog.service.busy; selectByMouse: true; placeholderText: "Model ID" }
                Label { text: "Prompt" }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 130
                    TextArea {
                        id: prompt
                        objectName: "aiPrompt"
                        placeholderText: "A tiny emerald dragon, green scales, golden eyes, small wings"
                        wrapMode: TextEdit.Wrap
                        enabled: !dialog.service.busy
                        selectByMouse: true
                    }
                }
                Label {
                    text: dialog.promptLength + " / 2000 characters"
                    color: dialog.promptLength > 2000 ? "#ff9b91" : "#9aabba"
                }
                Label { text: "API key (kept in memory for this session)"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                TextField {
                    id: apiKey
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    enabled: !dialog.service.busy
                    placeholderText: "Paste your API key here"
                    selectByMouse: true
                }
                CheckBox {
                    id: settingsToggle
                    text: "API Settings"
                    checked: !dialog.service.settings.authHeader
                }
                ColumnLayout {
                    visible: settingsToggle.checked
                    enabled: !dialog.service.busy
                    Layout.fillWidth: true
                    Label { text: "Generation URL (POST)" }
                    TextField { id: generationUrl; Layout.fillWidth: true; selectByMouse: true }
                    Label { text: "Authentication header" }
                    TextField { id: authHeader; Layout.fillWidth: true; placeholderText: "For example: Authorization or X-API-Key"; selectByMouse: true }
                    Label { text: "Key prefix (include any trailing space)" }
                    TextField { id: authPrefix; Layout.fillWidth: true; placeholderText: "For example: Bearer followed by a space, or empty"; selectByMouse: true }
                    Label { text: "Status URL (GET, must contain {jobId})"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    TextField { id: statusUrl; Layout.fillWidth: true; placeholderText: "Enter the actual status endpoint"; selectByMouse: true }
                    Label {
                        Layout.fillWidth: true
                        text: "AI Sprite Studio uses Authorization: Bearer and the endpoints configured by default. No API key is saved to disk."
                        wrapMode: Text.Wrap
                        color: "#9aabba"
                    }
                    Button { text: "Save API Settings"; onClicked: dialog.applySettings() }
                }
                Label {
                    Layout.fillWidth: true
                    text: "Each accepted generation charges the selected model's credit cost. Status checks and PNG downloads are free. Save your PNG: deleting it from Media removes the API result."
                    wrapMode: Text.Wrap
                    color: "#9aabba"
                }
                Label {
                    Layout.fillWidth: true
                    visible: dialog.service.jobId.length > 0
                    text: "Job: " + dialog.service.jobId
                    wrapMode: Text.WrapAnywhere
                    color: "#9aabba"
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Label { text: "Generated PNG"; font.bold: true }
            Checker {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Image {
                    id: preview
                    anchors.fill: parent
                    anchors.margins: 8
                    source: dialog.service.imageUrl
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    cache: false
                }
                Label { anchors.centerIn: parent; visible: !dialog.hasImage; text: "No generated image yet"; color: "#9aabba" }
            }
            Label { text: dialog.hasImage ? preview.sourceSize.width + " × " + preview.sourceSize.height + " px" : "" }
            RowLayout {
                Button { text: "Save PNG"; enabled: dialog.hasImage; onClicked: saveDialog.open() }
                Button {
                    text: "Open in Slicer"
                    enabled: dialog.hasImage
                    onClicked: { dialog.slicerRequested(dialog.service.imageUrl); dialog.close() }
                }
            }
        }
    }
}
