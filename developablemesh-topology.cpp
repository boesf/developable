#include "developablemesh.h"
#include "mathutil.h"
#include <Eigen/Dense>

#include <set>

using namespace Eigen;
using namespace std;

void DevelopableMesh::buildSchwarzLantern(double r, double h, int n, int m, double angle)
{
    mesh_ = OMMesh();
    double alpha = angle;
    double a = 2*r*sin(MathUtil::PI/n);
    double b = sqrt(4*r*r*sin(alpha/2.0)*sin(alpha/2.0) + h*h/m/m);
    double c = sqrt(4*r*r*sin(MathUtil::PI/n + alpha/2.0)*sin(MathUtil::PI/n + alpha/2.0) + h*h/m/m);
    double costilt = (a*a+b*b-c*c)/(2*a*b);
    double dpx = -b*costilt;


    double s = 0.5*(a+b+c);
    double area = sqrt(s*(s-a)*(s-b)*(s-c));
    double dpy = 2*area/a;

    double W = n*a;
    double H = 2*n*m*area/W;

    assert(fabs(H-dpy*m) < 1e-6);

    delete material_;
    material_ = new MaterialMesh(H);
    boundaries_.clear();

    Boundary bottom, top;

    for(int i=0; i<=m; i++)
    {
        double z = h*i/m;
        double basepx = i*dpx;
        double py = i*dpy;

        for(int j=0; j<n; j++)
        {
            double x = r*cos(2*MathUtil::PI*(j/double(n)) + i*alpha);
            double y = r*sin(2*MathUtil::PI*(j/double(n)) + i*alpha);

            OMMesh::Point newpt(x,y,z);
            if(i == 0)
            {
                bottom.bdryVerts.push_back(mesh_.n_vertices());
                bottom.bdryPos.push_back(Vector3d(x,y,z));
            }
            else if(i==m)
            {
                top.bdryVerts.push_back(mesh_.n_vertices());
                top.bdryPos.push_back(Vector3d(x,y,z));
            }
            mesh_.add_vertex(newpt);

            double px = j*a+basepx;
            OMMesh::Point newmatpt(px,py,0);
            if(i == 0 || i == m)
            {
                MaterialBoundary bd(material_);
                bd.vertid = material_->getMesh().n_vertices();
                bd.xpos = px;
                bd.onBottom = (i==0);
                material_->getBoundaryVerts().push_back(bd);
            }
            material_->getMesh().add_vertex(newmatpt);

        }
    }

    boundaries_.push_back(bottom);
    boundaries_.push_back(top);

    for(int i=0; i<m; i++)
    {
        for(int j=0; j<n; j++)
        {
            int fidx1 = i*n+j;
            int fidx2 = i*n + ((j+1) % n);
            int fidx3 = (i+1)*n+ ((j+1)%n);

            vector<OMMesh::VertexHandle> newface;
            vector<OMMesh::VertexHandle> newmatface;
            newface.push_back(mesh_.vertex_handle(fidx1));
            newface.push_back(mesh_.vertex_handle(fidx2));
            newface.push_back(mesh_.vertex_handle(fidx3));
            newmatface.push_back(material_->getMesh().vertex_handle(fidx1));
            newmatface.push_back(material_->getMesh().vertex_handle(fidx2));
            newmatface.push_back(material_->getMesh().vertex_handle(fidx3));

            mesh_.add_face(newface);
            material_->getMesh().add_face(newmatface);

            if(j == n-1)
            {
                // Wraps around
                Vector3d offset(W,0,0);
                int heid = material_->findHalfedge(fidx1, fidx2);
                assert(heid >= 0);
                material_->addOffset(material_->getMesh().halfedge_handle(heid), offset);
                heid = material_->findHalfedge(fidx1, fidx3);
                assert(heid >= 0);
                material_->addOffset(material_->getMesh().halfedge_handle(heid), offset);
            }

            material_->setMaterialEdge(findEdge(fidx1,fidx2), material_->findEdge(fidx1,fidx2));
            material_->setMaterialEdge(findEdge(fidx2,fidx3), material_->findEdge(fidx2,fidx3));
            material_->setMaterialEdge(findEdge(fidx3,fidx1), material_->findEdge(fidx3,fidx1));

            fidx2 = fidx3;
            newface[1] = mesh_.vertex_handle(fidx2);
            newmatface[1] = material_->getMesh().vertex_handle(fidx2);
            fidx3 = (i+1)*n + j;
            newface[2] = mesh_.vertex_handle(fidx3);
            newmatface[2] = material_->getMesh().vertex_handle(fidx3);
            mesh_.add_face(newface);
            material_->getMesh().add_face(newmatface);

            if(j == n-1)
            {
                // Wraps around
                Vector3d offset(W,0,0);
                int heid = material_->findHalfedge(fidx1, fidx2);
                assert(heid >= 0);
                material_->addOffset(material_->getMesh().halfedge_handle(heid), offset);
                heid = material_->findHalfedge(fidx3, fidx2);
                assert(heid >= 0);
                material_->addOffset(material_->getMesh().halfedge_handle(heid), offset);
            }

            material_->setMaterialEdge(findEdge(fidx1,fidx2), material_->findEdge(fidx1,fidx2));
            material_->setMaterialEdge(findEdge(fidx2,fidx3), material_->findEdge(fidx2,fidx3));
            material_->setMaterialEdge(findEdge(fidx3,fidx1), material_->findEdge(fidx3,fidx1));

        }
    }

    for(int i=0; i<(int)mesh_.n_edges(); i++)
    {
        double len1 = edgeLength(i);

        int medge = material_->materialEdge(i);

        double len2 = material_->edgeLength(medge);

        assert(fabs(len1-len2) < 1e-6);
    }

    for(int i=0; i<(int)material_->getMesh().n_faces(); i++)
    {
        OMMesh::FaceHandle fh = material_->getMesh().face_handle(i);
        for(OMMesh::FaceHalfedgeIter fhi = material_->getMesh().fh_iter(fh); fhi; ++fhi)
        {
            OMMesh::HalfedgeHandle heh = fhi.handle();
            OMMesh::HalfedgeHandle nextheh = material_->getMesh().next_halfedge_handle(heh);
            OMMesh::VertexHandle centv = material_->getMesh().to_vertex_handle(heh);
            OMMesh::VertexHandle v1 = material_->getMesh().from_vertex_handle(heh);
            OMMesh::VertexHandle v2 = material_->getMesh().to_vertex_handle(nextheh);

            OMMesh::Point e0 = material_->getMesh().point(v1) - material_->getMesh().point(centv);
            OMMesh::Point e1 = material_->getMesh().point(v2) - material_->getMesh().point(centv);
            Vector3d ve0 = point2Vector(e0) - material_->getOffset(heh);
            Vector3d ve1 = point2Vector(e1) + material_->getOffset(nextheh);
            Vector3d cr = ve0.cross(ve1);
            assert(cr[2] < 0);
        }
    }
    centerCylinder();

    //fix Boundaries
    for(int i=0; i<(int)boundaries_.size(); i++)
    {
        for(int j=0; j<(int)boundaries_[i].bdryVerts.size(); j++)
        {
            OMMesh::Point pt = mesh_.point(mesh_.vertex_handle(boundaries_[i].bdryVerts[j]));
            boundaries_[i].bdryPos[j] = point2Vector(pt);
        }
    }
    for(int i=0; i<(int)material_->getBoundaryVerts().size(); i++)
    {
        OMMesh::Point pt = material_->getMesh().point(material_->getMesh().vertex_handle(material_->getBoundaryVerts()[i].vertid));
        material_->getBoundaryVerts()[i].xpos = pt[0];
    }
}

void DevelopableMesh::collapseEdge(int eid)
{
    OMMesh::EdgeHandle eh = mesh_.edge_handle(eid);
    OMMesh::HalfedgeHandle heh = mesh_.halfedge_handle(eh,0);
    if(mesh_.is_boundary(mesh_.from_vertex_handle(heh)))
        heh = mesh_.opposite_halfedge_handle(heh);
    assert(!mesh_.is_boundary(mesh_.from_vertex_handle(heh)));

    int v2 = mesh_.to_vertex_handle(heh).idx();
    int v1 = mesh_.from_vertex_handle(heh).idx();

    int mheid = material_->findHalfedge(v1, v2);
    OMMesh::HalfedgeHandle mheh = material_->getMesh().halfedge_handle(mheid);
    Vector3d deletedoffset = material_->getOffset(mheh);

    vector<int> old2new;

    OMMesh newmesh;
    MaterialMesh *newmmesh = new MaterialMesh(material_->getH());

    int mergedid = -1;
    int newid = 0;
    for(int i=0; i<(int)mesh_.n_vertices(); i++)
    {
        if(i == v1)
        {
            old2new.push_back(-1);
            continue;
        }
        OMMesh::Point newvel;
        if(i == v2)
        {
            mergedid = newid;
            newvel = (mesh_.data(mesh_.vertex_handle(v1)).vel() + mesh_.data(mesh_.vertex_handle(v2)).vel())*0.5;
        }
        else
            newvel = mesh_.data(mesh_.vertex_handle(i)).vel();

        old2new.push_back(newid++);
        OMMesh::Point pt = mesh_.point(mesh_.vertex_handle(i));
        OMMesh::VertexHandle newvh = newmesh.add_vertex(pt);
        newmesh.data(newvh).set_vel(newvel);
        OMMesh::Point mpt = material_->getMesh().point(material_->getMesh().vertex_handle(i));
        newmmesh->getMesh().add_vertex(mpt);
    }

    old2new[v1] = mergedid;

    // Fix boundary data
    vector<Boundary> newbdries;
    for(int i=0; i<(int)boundaries_.size(); i++)
    {
        bool seen = false;
        Boundary newbd;
        for(int j=0; j<(int)boundaries_[i].bdryVerts.size(); j++)
        {
            if(boundaries_[i].bdryVerts[j] == v1 || boundaries_[i].bdryVerts[j] == v2)
            {
                if(seen)
                    continue;
                else
                    seen = true;
            }
            newbd.bdryPos.push_back(boundaries_[i].bdryPos[j]);
            newbd.bdryVerts.push_back(old2new[boundaries_[i].bdryVerts[j]]);
        }
        newbdries.push_back(newbd);
    }

    vector<MaterialBoundary> newBdryVerts;
    bool seen = false;
    for(int i=0; i<(int)material_->getBoundaryVerts().size(); i++)
    {
        MaterialBoundary entry = material_->getBoundaryVerts()[i];
        if(entry.vertid == v1 || entry.vertid == v2)
        {
            if(seen)
                continue;
            else
                seen = true;
        }
        MaterialBoundary newentry = entry;
        newentry.m_ = newmmesh;
        newentry.vertid = old2new[entry.vertid];
        newBdryVerts.push_back(newentry);
        newmmesh->getBoundaryVerts() = newBdryVerts;
    }

    // ignored faces
    int f1 = mesh_.face_handle(heh).idx();
    int f2 = mesh_.opposite_face_handle(heh).idx();

    for(int i=0; i<(int)mesh_.n_faces(); i++)
    {
        if(i == f1 || i == f2)
            continue;
        OMMesh::FaceHandle fh = mesh_.face_handle(i);
        vector<OMMesh::VertexHandle> newface;
        for(OMMesh::FaceVertexIter fvi = mesh_.fv_iter(fh); fvi; ++fvi)
        {
            int vid = fvi.handle().idx();
            OMMesh::VertexHandle vh = newmesh.vertex_handle(old2new[vid]);
            newface.push_back(vh);
        }
        newmesh.add_face(newface);
        OMMesh::FaceHandle mfh = material_->getMesh().face_handle(i);
        vector<OMMesh::VertexHandle> newmface;
        for(OMMesh::FaceVertexIter fvi = material_->getMesh().fv_iter(mfh); fvi; ++fvi)
        {
            int vid = fvi.handle().idx();
            OMMesh::VertexHandle vh = newmmesh->getMesh().vertex_handle(old2new[vid]);
            newmface.push_back(vh);
        }
        newmmesh->getMesh().add_face(newmface);
    }

    // Reset edge map
    for(OMMesh::EdgeIter ei = newmesh.edges_begin(); ei != newmesh.edges_end(); ++ei)
    {
        OMMesh::HalfedgeHandle heh = newmesh.halfedge_handle(ei.handle(), 0);
        int v1 = newmesh.from_vertex_handle(heh).idx();
        int v2 = newmesh.to_vertex_handle(heh).idx();
        int meid = newmmesh->findEdge(v1,v2);
        newmmesh->setMaterialEdge(ei.handle().idx(), meid);
    }

    // set edge offset (except v2's edges)
    for(OMMesh::HalfedgeIter hei = material_->getMesh().halfedges_begin(); hei != material_->getMesh().halfedges_end(); ++hei)
    {
        int oldv1 = material_->getMesh().from_vertex_handle(hei.handle()).idx();
        int oldv2 = material_->getMesh().to_vertex_handle(hei.handle()).idx();
        if(oldv1 == v2 || oldv2 == v2)
            continue;
        int newv1 = old2new[oldv1];
        int newv2 = old2new[oldv2];
        int newheid = newmmesh->findHalfedge(newv1,newv2);
        assert(newheid != -1);
        OMMesh::HalfedgeHandle heh = newmmesh->getMesh().halfedge_handle(newheid);
        newmmesh->addOffset(heh, material_->getOffset(hei.handle()));
    }

    // last order of business: fix the offsets of the edges coming off of v2
    OMMesh::VertexHandle source = material_->getMesh().vertex_handle(v2);
    for(OMMesh::VertexOHalfedgeIter voh = material_->getMesh().voh_iter(source); voh; ++voh)
    {
        int target = material_->getMesh().to_vertex_handle(voh.handle()).idx();
        if(target == v1)
            continue;
        Vector3d newoffset = deletedoffset + material_->getOffset(voh.handle());
        int newheid = newmmesh->findHalfedge(old2new[v1], old2new[target]);
        assert(newheid != -1);
        OMMesh::HalfedgeHandle newheh = newmmesh->getMesh().halfedge_handle(newheid);
        newmmesh->addOffset(newheh, newoffset);
    }

    mesh_ = newmesh;
    delete material_;
    material_ = newmmesh;
    boundaries_ = newbdries;
}

bool DevelopableMesh::canCollapseEdge(int eid)
{
    // Some sanity checks. First, the edge should not span the boundaries:
    OMMesh::EdgeHandle eh = mesh_.edge_handle(eid);
    OMMesh::HalfedgeHandle heh = mesh_.halfedge_handle(eh,0);
    OMMesh::VertexHandle source = mesh_.from_vertex_handle(heh);
    OMMesh::VertexHandle target = mesh_.to_vertex_handle(heh);

    if(mesh_.is_boundary(source) && mesh_.is_boundary(target))
        return false;

    // Next, the source and target verts should have only two other verts in common (otherwise collapsed mesh is nonmanifold)
    set<int> neighbors1;
    for(OMMesh::VertexVertexIter vvi = mesh_.vv_iter(source); vvi; ++vvi)
        neighbors1.insert(vvi.handle().idx());

    int duplicates=0;
    for(OMMesh::VertexVertexIter vvi = mesh_.vv_iter(target); vvi; ++vvi)
    {
        if(neighbors1.count(vvi.handle().idx()) > 0)
            duplicates++;
    }
    if(duplicates != 2)
        return false;

    // we're ok
    return true;
}

int DevelopableMesh::findCollapsibleEdge(const Eigen::VectorXd &q)
{
    const double constrtol = 0.01;

    for(OMMesh::HalfedgeIter hei = mesh_.halfedges_begin(); hei != mesh_.halfedges_end(); ++hei)
    {
        if(mesh_.is_boundary(hei.handle()))
            continue;
        double g;
        vector<pair<int, double> > dummy1;
        vector<T> dummy2;

        int v1 = mesh_.from_vertex_handle(hei.handle()).idx();
        int v2 = mesh_.to_vertex_handle(hei.handle()).idx();

        int hidx = material_->findHalfedge(v1, v2);
        assert(hidx != -1);

        OMMesh::HalfedgeHandle heh1 = material_->getMesh().halfedge_handle(hidx);

        radiusOverlapConstraint(q, heh1, g, dummy1, dummy2);

        if(fabs(g) < constrtol)
        {
            int candidate = mesh_.edge_handle(mesh_.prev_halfedge_handle(hei.handle())).idx();
            if(canCollapseEdge(candidate))
                return candidate;
        }
    }

    return -1;
}
