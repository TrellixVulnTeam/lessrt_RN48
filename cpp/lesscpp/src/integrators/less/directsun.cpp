#include "directsun.h"

MTS_NAMESPACE_BEGIN



MTS_IMPLEMENT_CLASS(SunDirectWorkUnit, false, WorkUnit)

MTS_IMPLEMENT_CLASS(SunDirectWorkResult, false, WorkResult)



void DirectSunWorkProcessor::process(const WorkUnit *workUnit, WorkResult *workResult,
const bool &stop) {
	const SunDirectWorkUnit *wu
		= static_cast<const SunDirectWorkUnit *>(workUnit);
	SunDirectWorkResult *wr = static_cast<SunDirectWorkResult *>(workResult);
	Vector2i size = wu->getSize();
	Point2i offset = wu->getOffset();
	Vector2i extended_size = wu->getExtendedSceneSize();
	int particleNum = wu->getParticleNum();
	float sunPlaneHeight = wu->getSunPlaneHeight();
	wr->setSize(size);
	wr->setOffset(offset);
	wr->setParticleNum(particleNum);
	Point2f real_offset = Point2f(0.5*extended_size.x - size.x*offset.y, 0.5*extended_size.y - size.y*offset.x);
	/*Ray sunRay;
	Point2 pos,aperpos;
	m_sensor->sampleRay(sunRay, pos, aperpos,sunRay.time);*/
	std::vector<float> m_theta = { 0, 30 };
	std::vector<float> m_phi = { 0, 90 };

	std::list<Point> intersected_points;
	for (int i = 0; i < size.y; i++)
	{
		for (int j = 0; j < size.x; j++)
		{
			for (int k = 0; k < particleNum; k++) //ÿƽ�ײ�������������
			{
				float x = -m_sampler->next1D();//����0-1����������
				float z = -m_sampler->next1D();//����0-1����������
				Point2f pos_in_block = Point2f(i + x, j + z);
				Point2f pos_in_scene = real_offset + pos_in_block;
				Ray ray;
				Point o = Point(pos_in_scene.x, sunPlaneHeight, pos_in_scene.y);
				//Point o = Point(0, sunPlaneHeight,0);
				ray.setOrigin(o);
				ray.setDirection(Vector(0, -1, 0));//��ʼ����
				Intersection its;
				bool isIntersected = m_scene->rayIntersect(ray, its);
				if (isIntersected)
				{
					//intersected_points.push_back(its.p);//����һ��ɢ�佻��
					for (int bound = 0; bound < 1; bound++)
					{
						const BSDF *bsdf = its.getBSDF();
						//͸����
						BSDFSamplingRecord bRectrans(its, Vector(0, 0, 1), Vector(0, 0, -1));
						bRectrans.typeMask = bsdf->EDiffuseTransmission;
						Spectrum trans = bsdf->eval(bRectrans) * M_PI;
						//������
						BSDFSamplingRecord bRecref(its, Vector(0, 0, 1), Vector(0, 0, 1));
						bRecref.typeMask = bsdf->EDiffuseReflection;
						Spectrum ref = bsdf->eval(bRecref) * M_PI;

						//�жϹ����Ƿ�����
						float rand = m_sampler->next1D();
						//�����µķ���
						Vector wo_local = warp::squareToCosineHemisphere(m_sampler->next2D());						
						Ray newray;
						if (rand <= ref[0])//����
						{
							const Vector wo = its.toWorld(wo_local);
							newray = Ray(its.p, wo, ray.time);
						}
						else if (rand > ref[0] && rand <= ref[0] + trans[0]) //͸��
						{
							wo_local.z *= -1;
							const Vector wo = its.toWorld(wo_local);
							newray = Ray(its.p, wo, ray.time);
						}
						else
						{
							break;
						}

						bool isIntersected_new = m_scene->rayIntersect(newray, its);
						if (isIntersected_new)
						{
							intersected_points.push_back(its.p);
						}
						else
						{
							break;//���볡���޽��㣬���˳�
						}

					}

				}

			}
		}
	}



	float* result = new float[intersected_points.size() * 3];
	std::list<Point>::iterator it = intersected_points.begin();
	//cout << "intersected_size:" << intersected_points.size() << endl;
	int index = 0;
	for (; it != intersected_points.end(); it++)
	{
		result[index++] = (*it).x;
		result[index++] = (*it).y;
		result[index++] = (*it).z;
	}
	wr->setPointNum(intersected_points.size());
	wr->setPointCloud(result);
}
MTS_IMPLEMENT_CLASS_S(DirectSunWorkProcessor, false, WorkProcessor)

MTS_IMPLEMENT_CLASS(DirectSunProcess, false, ParallelProcess)


MTS_NAMESPACE_END