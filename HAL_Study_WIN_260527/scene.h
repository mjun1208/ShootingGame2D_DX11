#ifndef SCENE_H
#define SCENE_H

class cScene abstract
{
public:
	virtual ~cScene() = default;

	virtual bool Initialize() { return true; }
	virtual void Finalize() {}
	virtual void Update(float delta_time) = 0;
	virtual void FixedUpdate() {}
	virtual void Draw() = 0;
};

#endif // !SCENE_H
