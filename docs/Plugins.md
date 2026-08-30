# 플러그인 구성

이 문서는 RS 프로젝트에서 활성화한 Unreal Engine 플러그인과 용도를 정리한다.

기준 환경은 Unreal Engine 5.8이며, 플러그인 활성화 상태는 `RS/RS.uproject`의 `Plugins` 항목에서 관리한다.

| 플러그인 | 설정 이름 | 용도 | 범위 |
| --- | --- | --- | --- |
| Modeling Tools Editor Mode | `ModelingToolsEditorMode` | Unreal Editor의 메시 편집 및 모델링 도구 | Editor 전용 |
| Gameplay Ability System | `GameplayAbilities` | Ability, Gameplay Effect, Attribute 기반 게임플레이 구현 | Runtime |
| Unreal MCP | `ModelContextProtocol` | 외부 MCP 클라이언트와 Unreal Editor 연결 | 개발 도구 |
| Editor Toolset | `EditorToolset` | MCP에서 사용할 에디터 조작 도구 제공 | Editor 전용 |

## 관리 원칙

- 플러그인을 추가하거나 제거할 때 `RS/RS.uproject`와 이 문서를 함께 수정한다.
- Runtime 코드에서 플러그인 API를 사용하면 `RS/Source/RS/RS.Build.cs`의 모듈 의존성도 확인한다.
- 실험 플러그인인 `ModelContextProtocol`과 `EditorToolset`은 엔진 버전 변경 후 다시 검증한다.
- MCP 설정과 사용 방법은 [MCP 사용 가이드](MCP.md)를 참고한다.
