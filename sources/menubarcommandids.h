#ifndef MENUBARCOMMANDIDS_H
#define MENUBARCOMMANDIDS_H

// コマンドIDのリスト

//---- メインウィンドウ メニューバーコマンド
//---- Sequence Editor メニューバーコマンド
// File Menu
#define MI_NewProject "MI_NewProject"
#define MI_OpenProject "MI_OpenProject"
#define MI_SaveProject "MI_SaveProject"
#define MI_SaveAsProject "MI_SaveAsProject"
#define MI_SaveProjectWithDateTime "MI_SaveProjectWithDateTime"
#define MI_OpenRecentProject "MI_OpenRecentProject"
#define MI_ClearRecentProject "MI_ClearRecentProject"
#define MI_ImportProject "MI_ImportProject"
#define MI_InsertImages "MI_InsertImages"
#define MI_ExportSelectedShapes "MI_ExportSelectedShapes"

#define MI_Preferences "MI_Preferences"
#define MI_Exit "MI_Exit"
// Edit Menu
#define MI_Undo "MI_Undo"
#define MI_Redo "MI_Redo"
#define MI_Cut "MI_Cut"
#define MI_Copy "MI_Copy"
#define MI_Paste "MI_Paste"
#define MI_Delete "MI_Delete"
#define MI_SelectAll "MI_SelectAll"
#define MI_Duplicate "MI_Duplicate"
#define MI_Key "MI_Key"
#define MI_Unkey "MI_Unkey"
#define MI_ResetInterpolation "MI_ResetInterpolation"
// View Menu
#define MI_ZoomIn "MI_ZoomIn"
#define MI_ZoomOut "MI_ZoomOut"
#define MI_ActualSize "MI_ActualSize"
// #define MI_Colors "MI_Colors"
//  Render Menu
#define MI_Preview "MI_Preview"
#define MI_Render "MI_Render"
#define MI_RenderOptions "MI_RenderOptions"
#define MI_OutputOptions "MI_OutputOptions"
#define MI_ShapeOptions "MI_ShapeOptions"
#define MI_TransformOptions "MI_TransformOptions"
#define MI_FreehandOptions "MI_FreehandOptions"
#define MI_RenderShapeImage "MI_RenderShapeImage"

// Shape Menu
#define MI_ToggleShapeLock "MI_ToggleShapeLock"
#define MI_ToggleVisibility "MI_ToggleVisibility"

//---- 上側の表示コントロールツールバーのコマンド
// #define VMI_MultiFrameMode "VMI_MultiFrameMode"

#define MI_InsertEmpty "MI_InsertEmpty"
#define MI_NextFrame "MI_NextFrame"
#define MI_PreviousFrame "MI_PreviousFrame"
// Timeline ファイルパスの差し替え
#define MI_ReplaceImages "MI_ReplaceImages"
// 再読み込み（キャッシュを消去）
#define MI_ReloadImages "MI_ReloadImages"
#define MI_FileInfo "MI_FileInfo"
#define MI_ClearMeshCache "MI_ClearMeshCache"
#endif