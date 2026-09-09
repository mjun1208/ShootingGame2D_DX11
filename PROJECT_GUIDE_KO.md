# 프로젝트 읽기 안내

현재 코드 정리 기준은 다음과 같습니다.

- 내부 고정 용량 저장소는 `std::array`, 가변 목록은 `std::vector`를 사용합니다.
- 조건부 목록 삭제는 `std::erase_if`, 첫 빈 슬롯 검색은 `std::find_if`로 표현합니다. ID 풀과 순환 덮어쓰기는 각 시스템의 생성·교체 정책에 따라 유지합니다.
- 공통 수학 처리는 `Engine/Core/math_utils`를 재사용합니다.
- `Constants` 폴더의 공유 상수는 `PascalCase`, 함수 인자는 `snake_case`를 기준으로 합니다.
- 헤더 중복 포함은 include guard로 막습니다. 외부 `ThirdParty` 코드는 별도로 유지합니다.
- `Youhei Sato` 주석이 있는 파일은 일관성 수정과 자동 포맷 대상에서 제외합니다. `.clang-format-ignore`에 제외 경로를 기록합니다.
- 조건문과 반복문의 본문에는 한 문장이어도 중괄호를 사용합니다.
- 몬스터·무기·라운드 데이터의 `Find` 계열 조회는 잘못된 인자나 로딩 전 접근에 `nullptr`를 반환합니다. 필수 조회인 `Get` 계열은 같은 상황에 `std::out_of_range`를 발생시키며 다른 데이터를 대신 반환하지 않습니다.
- `Update(float delta_time, ...)` 진입점에서 음수 시간을 0으로 보정합니다. 연출별 최대 프레임 시간 제한은 별도 정책으로 유지합니다.
- 광원 추가 함수는 추가하지 못한 경우에도 기존 개수를 보존하되, 반환값을 `0`부터 유효 용량 사이로 제한합니다.
- `Audio`가 효과음과 BGM의 WAV 로딩·재생·정지·자원 해제를 공유합니다. `Bgm`은 곡 선택과 페이드만 담당하며 별도 음량 설정 API는 없습니다. 페이드용 내부 gain과 기존 곡별 기본 음량 균형은 유지합니다.
- 초기화는 `Audio_Initialize → Bgm_Initialize`, 종료는 `Bgm_Finalize → Audio_Finalize` 순서입니다.

디자인 패턴·STL·알고리즘의 실제 사용 위치와 선택 이유는 [기술 설명서](C:/ShootingGame2D_DX11/TECHNICAL_GUIDE_KO.md)에 정리했습니다.

현재 프로젝트는 C++와 Windows API, DirectX 11로 만든 2D 게임입니다. 이 문서는 현재 소스 구조를 기준으로 작성했습니다. 처음부터 모든 파일을 순서대로 읽을 필요는 없습니다. 수정하려는 기능의 진입점부터 읽으면 됩니다.

## 1. 폴더별 역할

실제 게임 소스는 `HAL_Study_WIN_260527` 폴더 안에 있습니다.

```text
C:/ShootingGame2D_DX11/
├─ HAL_Study_WIN_260527.slnx   Visual Studio에서 여는 솔루션
├─ .clang-format             줄바꿈·들여쓰기 규칙
├─ PROJECT_GUIDE_KO.md        이 안내서
└─ HAL_Study_WIN_260527/
   ├─ Engine/                게임을 실행하는 공통 기능
   │  ├─ Core/               프로그램 시작, 시간, 충돌, 데이터 읽기
   │  ├─ Graphics/           DirectX, 이미지, 스프라이트, 카메라
   │  ├─ Input/              키보드, 마우스, 게임패드
   │  └─ Audio/              효과음, 배경음악
   ├─ Scene/                 타이틀, 게임 진행, 클리어·게임오버 화면
   ├─ Game/                  게임 규칙과 캐릭터 동작
   │  ├─ Player/             플레이어 이동·대시·성장, 데이터 관리
   │  ├─ Enemy/              적 생성·행동·공격·피격·그리기
   │  ├─ Weapon/             무기 데이터, 발사 방식, 강화
   │  ├─ Combat/             플레이어 탄 관리, 투사체 이동·충돌 연계
   │  ├─ Map/                맵 데이터·생성·그리기·벽 충돌
   │  ├─ Items/              경험치, 회복 아이템, 상자, 포탈
   │  └─ Effects/            타격 효과, 피, 피해 숫자, 번개, 시간 정지
   ├─ UI/                    HUD, 버튼, 일시정지·강화 메뉴
   ├─ Constants/             여러 파일이 공유하는 상수·설정
   ├─ asset/                 이미지, 소리, 폰트, JSON 데이터
   ├─ ThirdParty/            외부에서 가져온 코드
   └─ shader_*.hlsl          GPU에서 실행하는 그리기 코드
```

`Debug`, `x64` 같은 폴더는 빌드 결과를 담습니다. 루트의 `tmp`, `review-build`, `outputs` 등은 작업·검증 자료가 있으므로 게임 기능을 읽을 때는 건너뛰어도 됩니다. 파일을 삭제하라는 의미는 아닙니다.

## 2. 게임이 실행되는 흐름

```text
WinMain                         윈도우를 만들고 반복 실행 시작
  → Application_Initialize     입력, 소리, 그래픽, 장면 준비
  → 반복
      Application_Update       입력 갱신 → 현재 장면 Update
      Application_Draw         현재 장면 Draw
  → Application_Finalize       사용한 자원 정리
```

장면 관리자는 현재 화면 하나를 골라 그 화면의 `Update`와 `Draw`를 호출합니다. 게임 플레이 화면에서는 플레이어, 적, 탄, 아이템, 충돌, 연출을 연결합니다. 일시정지·시간 정지·보스 연출에 따라 일부 단계는 건너뜁니다.

| 순서 | 볼 파일 | 확인할 내용 |
|---|---|---|
| 시작 | [main.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Engine/Core/main.cpp) | `WinMain`, 프로그램 반복 실행 |
| 공통 실행 | [application.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Engine/Core/application.cpp) | 준비·갱신·그리기·종료 |
| 화면 선택 | [scene_system.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Scene/scene_system.cpp) | 현재 장면을 갱신하고 장면 전환 |
| 게임 연결 | [ingame_scene.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Scene/ingame_scene.cpp) | 플레이어·적·공격·충돌 호출 관계 |

`ingame_scene.cpp`는 연결 관계를 보는 파일입니다. 길기 때문에 처음부터 끝까지 정독하기보다 `Update`에서 필요한 기능의 호출을 찾으세요.

## 3. 무엇을 바꾸고 싶은지에 따른 입구

| 하고 싶은 일 | 먼저 볼 파일 |
|---|---|
| 플레이어 이동·대시 이해 | [game_player.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Player/game_player.cpp) |
| 플레이어 기본 설정 변경 | [player_constants.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Constants/player_constants.h) |
| 슬라임·박쥐 행동 이해 | [enemy_patterns_basic.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_patterns_basic.cpp) |
| 크툴루 행동 이해 | [enemy_pattern_cthulhu.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_pattern_cthulhu.cpp) |
| 적 출현·웨이브 이해 | [enemy_encounter.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_encounter.cpp) |
| 적의 기본 데이터 확인 | [monsters.json](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/asset/data/monsters.json) |
| 무기 데이터 확인 | [weapons.json](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/asset/data/weapons.json) |
| 무기 발사 과정 이해 | [weapon_fire_system.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Weapon/weapon_fire_system.cpp) |
| 발사된 탄의 움직임 이해 | [projectile.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Combat/projectile.cpp) |
| 방·복도 생성 이해 | [map_generation.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Map/map_generation.cpp) |
| 체력 표시 등 HUD 수정 | [ingame_hud.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/UI/ingame_hud.cpp) |

모든 조절값이 `Constants`에 있는 것은 아닙니다. JSON 데이터, 공용 상수, 파일·함수 내부 설정으로 나뉘어 있습니다.

## 4. 코드에서 사용하는 구현 기법

여기서 기법은 프로그래밍 방법을 뜻합니다. 전부 먼저 공부해야 한다는 뜻은 아닙니다.

| 기법 | 쉽게 말하면 | 프로젝트에서 볼 곳 |
|---|---|---|
| Update / Draw 분리 | 상태를 바꾸는 일과 화면에 그리는 일을 나눔 | `application.cpp`, 각 장면 |
| 장면 관리 | 타이틀·게임·결과 중 현재 화면을 선택 | `scene_system.cpp` |
| 상태 머신 | 준비 → 돌진 → 대기처럼 현재 행동에 따라 실행할 코드를 고름 | `ActionState`, `enemy_patterns_basic.cpp`의 `switch` |
| 무기별 분기 | 일반 발사·샷건·매직 블레이드 처리를 선택 | `weapon_fire_system.cpp`의 `SubmitWeaponFire` |
| 공통 함수 재사용 | 여러 발의 배치와 발사를 같은 함수에서 처리 | `FireWeaponVolley`, `weapon_fire_context.h` |
| 풀·슬롯 재사용 | 탄 등을 위한 공간을 확보하고 빈 칸을 다시 사용 | `indexed_slot_pool.h`, `projectile.cpp` |
| 싱글턴 | 하나의 관리 객체에 공통 접근 | `GameDataManager::GetInstance()` |
| JSON 데이터 로딩 | 일부 게임 설정을 C++ 밖의 데이터 파일에서 읽음 | `game_data_manager`, `weapon_data_system.cpp` |
| `unique_ptr` | 객체의 소유자를 정하고 수명 종료 시 자동 정리 | 장면·상자 |
| `constexpr` 설정 | 실행 중 바꾸지 않는 값을 이름으로 표현 | 상수 헤더와 각 구현 파일 |

처음에는 **Update / Draw, 장면 관리, 상태 머신** 세 가지를 실제 코드와 함께 보면 됩니다.

## 5. `.h`, `.cpp`, namespace 읽는 법

- `.h`: 다른 코드가 사용할 함수·클래스·자료형의 선언을 주로 담습니다. 일부 짧은 구현도 들어 있습니다.
- `.cpp`: 함수가 실제로 무엇을 하는지 구현합니다.
- `namespace GamePlayer`: 플레이어 관련 이름들을 묶습니다. 파일이나 실행 순서를 뜻하지는 않습니다.
- 이름 없는 `namespace { ... }`: 해당 `.cpp` 안에서만 쓰는 코드입니다.
- `Internal`: 기능을 구현하는 파일끼리 쓰는 내부 자료형·함수라는 관례입니다.
- `Initialize`: 준비, `Update`: 시간에 따른 상태 변경, `Draw`: 그리기, `Finalize`: 정리입니다.
- `delta_time`: 지난 갱신 이후 흐른 시간입니다. 보통 이동량을 `속도 × 시간`으로 계산하는 데 씁니다.

## 6. 최근 상수 정리에서 달라진 점

최근 확인한 상수 309개 중 234개를 공용 헤더에서 사용하는 `.cpp`로 옮겼습니다. 그중 174개는 사용하는 함수 안으로 옮겼고, 슬라임·박쥐 돌진 값 9개는 설정 객체 2개로 묶었습니다. 빈 공용 헤더 13개도 제거했습니다.

309개는 이번 정리 대상이지 프로젝트 전체 상수 개수는 아닙니다. 설정 숫자를 대거 없앤 것이 아니라 다른 파일에서 알아야 하는 이름을 줄인 작업입니다. 이후 줄 정리는 값이나 코드 토큰을 바꾸지 않는 포맷 작업이었습니다.

## 7. 최근 작업에서 AI가 사용한 스킬과 도구

이 대화의 상수 정리·줄 정리·안내서 작성에서는 별도의 `SKILL.md` 스킬을 불러와 사용하지 않았습니다. 이전의 다른 작업 전체 이력까지 확인한 설명은 아닙니다.

| 실제 사용한 도구 | 용도 |
|---|---|
| `rg`, PowerShell | 파일·사용처 탐색, 소스 읽기 |
| `apply_patch`, PowerShell 스크립트 | 상수 이동, 참조 수정, 문서 작성 |
| MSBuild | 상수 이동 후 Debug x64 컴파일·링크 확인 |
| clang-format | 줄바꿈·들여쓰기 정리, 포맷 검사 |
| 비교 스크립트 | 값·문자열 보존, 포맷 전후 코드 토큰 동일성 확인 |

clang-format 규칙은 루트 `.clang-format`에 저장했습니다. 별도 에이전트나 외부 플러그인으로 이 작업을 나눠 수행하지 않았습니다. 마지막 포맷 작업은 토큰 비교와 포맷 검사로 검증했고, 그 이후 게임 실행 테스트를 했다는 뜻은 아닙니다.

## 8. 처음 읽을 순서

1. `application.cpp`에서 `Initialize`, `Update`, `Draw`, `Finalize`의 역할을 봅니다.
2. `ingame_scene.cpp`에서 `GamePlayer::Update` 호출을 찾습니다.
3. `game_player.cpp`에서 이동 처리만 따라갑니다. 대시·공격까지 한 번에 읽지 않아도 됩니다.
4. `player_constants.h`에서 `MoveSpeed`를 찾고 사용처로 돌아옵니다.
5. 익숙해지면 `enemy_patterns_basic.cpp`의 `UpdateSlime`과 `SlimeDash`를 봅니다.

각 기능에서 먼저 확인할 것은 세 가지입니다: **누가 호출하는가, 어떤 상태가 바뀌는가, 어떤 값으로 조절하는가.**
