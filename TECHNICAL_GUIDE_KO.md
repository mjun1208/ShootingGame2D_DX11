# 이 프로젝트의 디자인 패턴·STL·알고리즘

이 문서는 현재 C++ 소스에서 확인한 주요 구현을 설명합니다. **사용 위치와 동작은 소스에서 확인한 사실입니다. 선택 이유는 명시적인 코드 주석이 있는 경우를 제외하면 현재 구현의 효과를 바탕으로 해석한 것이며, 최초 작성자의 의도를 확인한 기록은 아닙니다.** 속도 개선 효과를 실제로 벤치마크한 문서도 아닙니다.

## 먼저 세 종류를 구분하기

- **디자인 패턴:** 객체와 코드의 역할을 어떻게 나눌 것인가. 예: 무기마다 발사 방법을 따로 구현하기.
- **STL·표준 라이브러리:** 데이터를 담거나 다루는 C++ 도구. 예: 가변 배열 `vector`, 큐 `queue`. `unique_ptr`와 난수 도구도 여기서 함께 다루지만 좁은 의미의 STL 컨테이너는 아닙니다.
- **알고리즘:** 문제를 해결하는 처리 순서. 예: BFS로 시작 방에서 다른 방까지 거리를 구하기.

같은 코드에 셋이 함께 등장할 수 있습니다. 맵에서는 `vector`로 방과 이웃 정보를 저장하고, `queue`를 이용해 BFS를 수행합니다.

## 1. 디자인 패턴과 설계 기법

### 무기별 분기와 공통 발사 함수

**위치:** [weapon_fire_context.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Weapon/weapon_fire_context.h), [weapon_fire_system.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Weapon/weapon_fire_system.cpp), [weapon_projectile_system.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Weapon/weapon_projectile_system.cpp)

`SubmitWeaponFire()`가 `switch`로 발사 처리를 선택합니다. 일반 무기는 한 발을 직접 발사하거나 `FireWeaponVolley()`로 여러 발을 배치합니다. 샷건은 같은 함수에서 각도·속도·수명에 난수 변화를 적용합니다. 매직 블레이드는 `WeaponMagicBlade::Fire()`에서 좌우 소환 위치와 연출을 처리하고, `Reset()`으로 좌우 교대 상태를 초기화합니다.

`WeaponFireContext`는 발사에 필요한 위치·방향·무기 설정을 묶고 공통 투사체 생성 함수를 호출합니다. 무기별 전략 객체를 생성하거나 등록할 필요 없이 호출 흐름을 따라갈 수 있습니다. 새로운 특수 발사 방식이 생기면 이 분기와 해당 처리 함수를 수정합니다.

### 장면 생성 함수 — 장면 객체를 만드는 곳 통일

**위치:** [scene_system.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Scene/scene_system.cpp)의 `CreateScene`, `SceneManager_Update`.

`CreateScene()`의 `switch`에서 `SceneID`에 맞는 장면을 생성합니다. 클리어 화면에는 기록된 클리어 시간을 전달합니다. 현재 장면과 전환 예약 상태는 구현 파일 안에 두고, 외부에서는 `SceneManager_*` 함수로 접근합니다. 전환 요청은 현재 장면의 `Update()`가 끝난 뒤 적용하므로 실행 중인 장면을 중간에 제거하지 않습니다.

### 유한 상태 머신(FSM) — 적의 행동 단계 구분

**위치:** [enemy_attack_pattern_internal.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_attack_pattern_internal.h)의 `ActionState`, [enemy_patterns_basic.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_patterns_basic.cpp).

슬라임은 `SlimeCooldown → SlimeTelegraph → SlimeDash` 같은 상태를 가집니다. `switch(runtime.State)`로 현재 상태의 처리만 실행하고 타이머·거리 조건에 따라 다음 상태로 바뀝니다.

**이 구조의 이유:** 대기, 공격 예고, 실제 돌진을 시간 순서대로 구분하기 쉽습니다. 대신 상태가 많아지면 분기와 전환 조건도 늘어납니다. 현재 형태는 enum과 switch 기반 FSM이며, 상태별 클래스를 만드는 GoF State 패턴과는 구별해야 합니다.

### 싱글턴 — 하나의 관리자에 접근

**위치:** [singleton.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Engine/Core/singleton.h), [game_data_manager.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Player/game_data_manager.h).

`GetInstance()` 안의 정적 객체를 반환하고 복사·이동을 금지합니다. 게임 데이터 관리자에 여러 곳에서 접근할 수 있습니다. 매번 관리자 참조를 전달하지 않아도 되지만, 어떤 코드가 관리자에 의존하는지 함수 인자만 보고 알기 어렵고 테스트에서 대체하기도 까다롭습니다.

### 객체 풀·슬롯 재사용 — 탄을 반복해서 생성·제거

**위치:** [indexed_slot_pool.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Engine/Core/indexed_slot_pool.h), [projectile.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Combat/projectile.cpp).

투사체 저장 배열과 빈 슬롯 ID를 재사용합니다. `IndexedSlotPool` 자체는 객체를 생성하는 관리자가 아니라 **사용 중인 ID와 빈 ID를 관리하는 도구**입니다. 투사체 배열과 결합해 풀처럼 동작합니다.

**이 구조의 이유:** 탄이 생기고 사라질 때마다 힙 객체를 할당·해제하는 일을 피하고, 활성 탄 ID만 순회할 수 있습니다. 대신 최대 용량이 정해지며 가득 차면 `Acquire()`가 무효 ID를 반환합니다. 반납된 ID도 나중에 재사용되므로 오래 저장한 ID를 안전한 영구 식별자로 취급하면 안 됩니다.

### Pimpl — 연출 클래스의 구현 숨기기

**위치:** [ingame_combat_presentation.h](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Scene/ingame_combat_presentation.h), [ingame_presentations.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Scene/ingame_presentations.cpp).

헤더에는 `struct Impl;`와 `std::unique_ptr<Impl>`만 두고 실제 내부 자료는 구현 파일에 둡니다. 다른 파일이 연출 내부의 세부 자료형을 알 필요를 줄입니다. 대신 동작을 읽으려면 구현 파일까지 이동해야 합니다. 상태가 작은 `BossIntroPresentation`은 private 멤버를 직접 보유합니다.

### 데이터와 동작 분리

**위치:** `asset/data/*.json`, `GameDataManager`, `weapon_data_system.cpp` 등.

일부 무기·몬스터·성장 설정은 JSON에서 읽고, 동작은 C++로 구현합니다. 데이터 변경과 동작 코드 수정을 나눌 수 있습니다. 대신 모든 설정이 JSON에 있는 것은 아니므로 값을 찾을 때 JSON, 공용 상수, 함수 내부 설정 중 어디에 속하는지 확인해야 합니다.

## 2. STL·표준 라이브러리 사용 이유

| 도구 | 실제 사용 예 | 적합한 이유 | 알아둘 점 |
|---|---|---|---|
| `std::array<T, N>` | 슬롯 ID 배열, 무기 전략 목록, 광원 배열 | 개수가 정해져 있고 인덱스로 바로 접근 | 크기가 고정됨. `array` 자체가 무조건 스택에 놓이는 것은 아님 |
| `std::vector<T>` | 방 목록, 방의 이웃 목록, 그리기용 인스턴스 목록 | 실행 중 개수가 달라짐. 연속 저장이라 순회하기 편함 | 커지며 재할당되면 기존 참조·포인터가 무효화될 수 있음 |
| `vector::reserve()` | `map_generation.cpp`의 렌더 배치 준비 | 예상 용량을 미리 확보해 반복 재할당을 줄임 | `size()`를 늘리거나 원소를 생성하는 함수는 아님 |
| `std::queue<int>` | `ComputeRoomDepths()`의 방문 대기 방 | 먼저 넣은 방부터 꺼내 BFS 순서 유지 | 앞에서 꺼내는 FIFO 방식 |
| `std::unordered_map<int, vector<SpriteInstance>>` | `projectile.cpp`의 텍스처별 탄 배치 | 텍스처 ID에 해당하는 그리기 묶음을 찾음 | 평균 조회 O(1), 최악 O(n). 키 순서로 순회되지 않음 |
| `std::unique_ptr<T>` | 현재 장면, 상자, 글자 객체, Pimpl | 객체 소유자를 하나로 정하고 소멸 시 자동 해제 | 자동으로 GPU 자원 정리까지 보장하는 것은 아님. 해당 클래스의 소멸·Finalize 구현에 달림 |
| `std::make_unique` | 장면·상자·전략 생성 | 생성된 객체의 소유권을 곧바로 관리 | 공유 소유가 아니라 단독 소유 |
| `std::sort` + 람다 | `enemy_renderer.cpp`의 광원 후보 정렬 | 카메라와 가까운 후보부터 제한된 슬롯에 배치 | 후보 수 n에 대해 O(n log n) 비교 |
| `std::find`, `std::find_if` | 방 연결 중복 확인, 회복 아이템 빈 칸 찾기 | 간단한 조건 검색을 짧게 표현 | 정렬·해시 검색이 아닌 선형 검색 O(n) |
| `std::clamp`, `min`, `max` | 체력·효과 진행률·라운드 값 제한 | 허용 범위를 명확히 표현 | 값의 의미나 입력 유효성 전체를 검증해 주지는 않음 |
| `std::mt19937`, 균등 분포 | `random_utils.cpp`의 정수·실수 난수 | 재사용하는 난수 엔진에서 필요한 범위의 값을 뽑음 | 현재 엔진은 `random_device`로 초기화되어 고정 시드 재현 흐름은 아님 |

`std::array<unique_ptr<전략>, 무기개수>`는 “종류 수는 고정, 각 칸이 서로 다른 전략 객체를 소유”하는 구조입니다. `vector<unique_ptr<Chest>>`는 “상자 개수는 변하고, 각 상자의 소유자는 이 목록”이라는 뜻입니다.

## 3. 알고리즘과 동작 원리

### A. 맵 생성: 유효한 확장 후보에서 무작위 선택

**위치:** [map_generation.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Map/map_generation.cpp)의 `GenerateRooms`, `BuildRoomConnections`.

1. 시작 방을 만듭니다.
2. 기존 방의 상하좌우에서 다음 방을 둘 수 있는 후보를 모읍니다.
3. 격자 범위·점유·배치 조건을 통과한 후보 하나를 무작위로 고릅니다.
4. 새 방과 부모 방을 기록하고 반복합니다.
5. 부모 기록으로 양방향 연결을 만듭니다.

부모와 연결하면서 방을 추가하므로 연결 관계를 관리하기 쉽고, 무작위 선택으로 배치에 변화를 줍니다. 후보가 없어지면 목표 개수 이전에도 중단할 수 있습니다. 보스 라운드는 별도의 고정 배치를 사용합니다. 이 구현을 BSP나 최소 신장 트리 알고리즘이라고 부르는 것은 맞지 않습니다.

### B. BFS: 시작 방에서 몇 번 이동해야 하는지 계산

**위치:** 같은 파일의 `ComputeRoomDepths`.

시작 방의 깊이를 0으로 정하고 큐에 넣습니다. 방을 하나 꺼내 아직 방문하지 않은 이웃의 깊이를 현재 깊이 + 1로 기록합니다. 이후 깊이, 말단 방 여부, 추가 배치 가능성 등의 조건을 사용해 최종 전투 위치 선택에 활용합니다.

**BFS를 쓰는 이유:** 방 연결마다 비용이 같은 상황에서 최소 연결 횟수를 구할 수 있습니다. 물리적으로 몇 픽셀 떨어졌는지가 아니라 몇 개의 연결을 지나야 하는지입니다. 방 수 V, 연결 수 E라면 BFS 부분은 O(V + E)입니다. 몬스터가 플레이어를 쫓는 경로 탐색과는 별개의 용도입니다.

### C. 공간 해시: 가까운 충돌 후보만 검사

**위치:** [collision.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Engine/Core/collision.cpp), [enemy_spatial.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_spatial.cpp).

월드 좌표를 격자 칸 좌표로 바꾸고, 해시로 버킷 번호를 구해 그 칸의 물체들을 연결합니다. 충돌 검사 때는 주변 칸에 있는 후보를 가져옵니다. 서로 다른 칸이 같은 버킷을 사용할 수 있으므로 실제 칸 좌표도 확인합니다.

**이유:** 모든 물체 쌍을 비교하는 방식의 후보 수는 n(n-1)/2입니다. 격자로 먼 물체를 제외하면 분산된 상황에서 검사량을 줄일 수 있습니다. 이것만으로 정확한 충돌이 결정되는 것은 아니며, 후보에 대해 원 충돌 등을 추가 검사합니다. 한곳에 몰리면 여전히 많은 비교가 필요하므로 항상 O(n)이라고 설명하면 안 됩니다.

### D. 원 충돌과 거리 제곱 비교

**위치:** `collision.cpp`의 `Collision_IsHitCircle`, 적의 거리 판정.

```cpp
dx * dx + dy * dy <= (radiusA + radiusB) * (radiusA + radiusB)
```

두 원 중심 사이 거리가 반지름 합 이하인지 확인하는 원리입니다. 거리 자체를 구하지 않고 제곱을 비교하므로 `sqrt`가 필요 없습니다. 원 두 개의 판정은 O(1)입니다. 사각형 스프라이트 모양이나 불투명 픽셀 윤곽과 정확히 같은 충돌 모양은 아닙니다.

### E. 경로 샘플링: 이동 중 벽 검사

**위치:** [map_collision.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Map/map_collision.cpp)의 `ProceduralMap_IsSegmentWalkable`, `ProceduralMap_TraceWalkableSegment`.

이동 시작점과 끝점 사이를 나누고 각 지점에서 원이 지나갈 수 있는지 검사합니다. `IsSegmentWalkable`은 거리/12를 올림한 횟수를 기준으로 구간을 나눕니다. `TraceWalkableSegment`는 진행하다 막히면 멈춥니다.

**이유:** 도착점만 검사하면 중간 벽을 놓칠 수 있어 이동 구간도 검사합니다. 다만 해석적인 연속 충돌 검출이 아니라 샘플링 방식이므로 모든 크기·속도 조건에서 관통 방지를 수학적으로 보장하는 것으로 설명해서는 안 됩니다. 이동 거리가 길면 검사 지점 수도 늘어납니다.

### F. 3차 베지어 곡선: 휘어지는 유도탄

**위치:** [projectile.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Combat/projectile.cpp)의 베지어 구간 생성·이동 처리.

시작점 P0, 두 제어점 P1·P2, 목표점 P3로 곡선을 만듭니다.

```text
B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t)t^2 P2 + t^3 P3
```

시간에 따라 t를 0에서 1로 진행시켜 탄 위치를 계산합니다. 목표가 이동하면 끝점과 두 번째 제어점도 보정합니다. 직선과 다른 휘어지는 공격 연출을 만들기 위한 방식입니다. t가 일정 속도로 증가해도 곡선 위 실제 이동 속도가 일정하다는 뜻은 아닙니다. 벽을 피해 최적 경로를 찾는 알고리즘도 아닙니다.

### G. 빈 슬롯 스택 + 마지막 원소로 빈자리 채우기

**위치:** `IndexedSlotPool::Acquire`, `Release`.

빈 ID 목록에서 하나를 꺼내 활성 ID 목록 끝에 넣습니다. 해제할 때는 활성 목록의 마지막 ID를 해제 위치로 옮기고, 해제한 ID를 빈 목록에 돌려줍니다. ID에서 활성 목록 위치로 가는 역색인도 함께 갱신합니다.

**이유:** 중간 원소를 지울 때 뒤의 모든 원소를 당기지 않아도 됩니다. 할당·반납은 O(1), 초기화는 용량에 비례하고 활성 목록 순회는 활성 개수에 비례합니다. 대신 활성 목록의 순서는 보존되지 않습니다.

### H. 텍스처별 배치와 인스턴싱

**위치:** `projectile.cpp`의 그리기 처리, [sprite_instanced.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Engine/Graphics/sprite_instanced.cpp).

같은 텍스처를 쓰는 탄의 위치·크기 등 인스턴스 정보를 모아 묶음으로 그립니다. 개별 탄마다 동일한 그림 설정을 반복하는 부담과 그리기 호출을 줄이려는 렌더링 기법입니다. 컨테이너로 묶는 것과 GPU 인스턴싱 호출은 연결되어 있지만 서로 다른 단계입니다. 투명 이미지의 겹침 순서가 중요한 경우에는 배치 순서도 고려해야 합니다.

### I. 거리순 정렬: 제한된 광원 선택

**위치:** [enemy_renderer.cpp](C:/ShootingGame2D_DX11/HAL_Study_WIN_260527/Game/Enemy/enemy_renderer.cpp)의 `LightCandidate` 정렬.

후보를 카메라와의 거리 제곱 오름차순으로 정렬한 뒤 남은 광원 슬롯 수만큼 선택합니다. 광원을 무제한 추가할 수 없으므로 가까운 후보를 우선하는 구조입니다. 모든 후보 정렬은 O(n log n)이고, 일부만 필요할 때 쓸 수 있는 부분 선택 방식보다 일을 더 할 수 있습니다.

## 4. 한 기능에서 어떻게 함께 쓰이는가

**플레이어 탄을 발사하고 그리는 흐름**으로 연결해 보세요.

```text
무기에 맞는 발사 처리 선택      switch + 공통 발사 함수
  → 투사체 설정 생성            무기별 발사 동작과 WeaponFireContext
  → 투사체 슬롯 확보            빈 ID 재사용
  → 매 프레임 위치 갱신         직선 또는 베지어 등
  → 벽·적과 충돌 판정           구간 샘플링, 공간 해시, 원 충돌
  → 같은 텍스처끼리 수집        unordered_map + vector
  → 묶어서 그리기               인스턴싱
  → 수명 종료 시 슬롯 반납       마지막 ID로 빈자리 채우기
```

패턴·자료구조·알고리즘의 이름을 외우기보다 각 단계가 **어떤 일을 맡는지**를 연결하면 읽기 쉽습니다.

## 5. 학습 우선순위

1. **array / vector + 반복문:** 현재 어떤 데이터가 몇 개 있는지 읽기.
2. **enum / switch 상태 머신:** 슬라임 한 종류의 행동 순서 읽기.
3. **queue + BFS:** 방의 깊이가 어떻게 계산되는지 읽기.
4. **unique_ptr + 장면 생성 함수:** 장면 객체의 소유권과 생성 흐름 읽기.
5. **슬롯 풀 + 공간 해시:** 탄·적이 많아질 때 처리량을 줄이는 방법 읽기.
6. **베지어·인스턴싱:** 움직임 표현과 렌더링 처리 읽기.

현재 구조를 그대로 유지해야 한다는 뜻은 아닙니다. 팩토리·Pimpl은 역할을 나누는 장점이 있지만 파일을 더 따라가야 하고, 싱글턴은 접근이 편한 대신 의존성이 숨습니다. 이해하기 어려운 부분은 실제 사용 범위와 수정 빈도를 보고 단순화할 수 있습니다.
